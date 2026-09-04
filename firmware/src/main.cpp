#include <Arduino.h>
#include "BoardConfig.h"
#include "Credentials.h"
#include "MeterConfig.h"
#include "IMeterReader.h"
#include "MiddeDTS27Reader.h"
#include "ElsterA150Reader.h"
#include "MiddeDDS26DReader.h"
#include "ElsterA1052Reader.h"
#include "BitPacker.h"
#include "LoRaWAN_Handler.h"
#include "DebugSerial.h"

// Instancia global de manejo LoRaWAN
LoRaWANHandler loraHandler;

// Instancia estática del Lector de Medidor según MeterConfig.h
#if SELECTED_METER_MODEL == METER_MODEL_MIDDE_DTS27
static MiddeDTS27Reader s_meterReader(IR_RX_PIN, IR_TX_PIN);
#elif SELECTED_METER_MODEL == METER_MODEL_ELSTER_A150
static ElsterA150Reader s_meterReader(IR_RX_PIN, IR_TX_PIN);
#elif SELECTED_METER_MODEL == METER_MODEL_MIDDE_DDS26D
static MiddeDDS26DReader s_meterReader(IR_RX_PIN, IR_TX_PIN);
#elif SELECTED_METER_MODEL == METER_MODEL_ELSTER_A1052
static ElsterA1052Reader s_meterReader(IR_RX_PIN, IR_TX_PIN);
#else
#error "Modelo de medidor no seleccionado o no soportado en MeterConfig.h"
#endif

static IMeterReader &g_reader = s_meterReader;

// Buffers estáticos para proteger el Stack Cortex-M3 (evita desbordes y HardFaults)
static MeterData g_meterData;
static uint8_t   g_binPayload0[32];
static uint8_t   g_binPayload1[32];

// Estructura de Diagnóstico en RAM para lectura SWD/OpenOCD
struct __attribute__((packed)) OpticalDiag {
  uint32_t magic;         // 0x0771C41D
  uint16_t voltajeA;
  uint16_t voltajeB;
  uint16_t voltajeC;
  uint16_t corrienteA;
  uint16_t corrienteB;
  uint8_t  cosphi;
  uint16_t frecuencia;
  uint32_t energiaActiva;
  uint8_t  lecturaOk;
  uint8_t  padding[3];
};

volatile OpticalDiag g_optDiag = {0};

// Control de LEDs en Lógica Active-LOW (LOW = Encendido, HIGH = Apagado)
void setLed(uint8_t pin, bool turnOn) {
  digitalWrite(pin, turnOn ? LOW : HIGH);
}

void blinkLed(uint8_t pin, uint8_t times, uint16_t delayMs) {
  for (uint8_t i = 0; i < times; i++) {
    setLed(pin, true);
    delay(delayMs);
    setLed(pin, false);
    delay(delayMs);
  }
}

void initLeds() {
  pinMode(LED_2_PIN, OUTPUT);
  pinMode(LED_3_PIN, OUTPUT);
  pinMode(LED_4_PIN, OUTPUT);
  pinMode(LED_MCU_PIN, OUTPUT);

  setLed(LED_2_PIN, false);
  setLed(LED_3_PIN, false);
  setLed(LED_4_PIN, false);
  setLed(LED_MCU_PIN, false);
}

// Handlers de excepciones con trampa visual (evita bucles de reinicio ciegos)
extern "C" __attribute__((used)) void HardFault_Handler(void) {
  while (1) {
    digitalWrite(LED_3_PIN, LOW);
    for (volatile int i = 0; i < 500000; i++);
    digitalWrite(LED_3_PIN, HIGH);
    for (volatile int i = 0; i < 500000; i++);
  }
}
extern "C" __attribute__((used)) void BusFault_Handler(void) { HardFault_Handler(); }
extern "C" __attribute__((used)) void UsageFault_Handler(void) { HardFault_Handler(); }

// Configuración de reloj resiliente con fallback a HSI
extern "C" void SystemClock_Config(void) {
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE | RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL4;

  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) {
    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
    RCC_OscInitStruct.HSIState = RCC_HSI_ON;
    RCC_OscInitStruct.HSEState = RCC_HSE_OFF;
    RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI_DIV2;
    RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL12;
    HAL_RCC_OscConfig(&RCC_OscInitStruct);
  }

  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK
                              | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
  HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2);
}

void setup() {
  debugSerialInit(115200);
  initLeds();

  debugPrintln("\r\n==================================================");
  debugPrintln("===   ElectroKaptor ME_LoRa_v3.6 Firmware     ===");
  debugPrintln("==================================================");

  // 1. Parpadeo inicial de prueba de todos los LEDs para confirmar encendido
  blinkLed(LED_2_PIN, 2, 80);
  blinkLed(LED_3_PIN, 2, 80);
  blinkLed(LED_MCU_PIN, 3, 80);

  debugPrintf("[INFO] Medidor Activo: %s\n", g_reader.getMeterName());

  // 2. Inicializar módem LoRaWAN RAK3172 en PB6/PB7
  debugPrintln("[INFO] Inicializando módem RAK3172 LoRaWAN...");
  if (!loraHandler.begin()) {
    debugPrintln("[ERROR] Módem RAK3172 no responde. Verificando alimentacion/reset...");
    blinkLed(LED_3_PIN, 5, 100);
  }

  // 3. Iniciar Unión OTAA a la Red TTN en Banda AU915 FSB2
  debugPrintln("[INFO] Iniciando autenticación OTAA Join en AU915 FSB2...");
  setLed(LED_2_PIN, false);
  if (!loraHandler.joinOTAA(8000)) {
    debugPrintln("[ALERTA] No se recibió confirmación inmediata de Join. El módem continuará auto-join en background.");
    blinkLed(LED_3_PIN, 3, 100);
  }
}

void loop() {
  static unsigned long lastTx = 0;
  static uint32_t cycleCount = 0;

  // Parpadeo Heartbeat en LED MCU (PC13)
  setLed(LED_MCU_PIN, true);
  delay(50);
  setLed(LED_MCU_PIN, false);

  // Ciclo de Lectura y Transmisión de Telemetría cada 15 segundos
  if (millis() - lastTx > 15000 || lastTx == 0) {
    lastTx = millis();
    cycleCount++;

    debugPrintf("\r\n==================================================\n");
    debugPrintf("=== [CICLO LECTURA Y TELEMETRÍA #%lu] ===\n", (unsigned long)cycleCount);
    debugPrintf("==================================================\n");

    // 1. INTERROGACIÓN AL MEDIDOR POR EL PUERTO ÓPTICO (PA3 RX / PB10 TX)
    debugPrintf("[LECTURA] Interrogando al medidor %s en PA3(RX) / PB10(TX)...\n", g_reader.getMeterName());
    bool readOk = g_reader.readMeter(g_meterData, 35000);

    g_optDiag.magic = 0x0771C41D;
    g_optDiag.voltajeA = g_meterData.voltajeA;
    g_optDiag.voltajeB = g_meterData.voltajeB;
    g_optDiag.voltajeC = g_meterData.voltajeC;
    g_optDiag.corrienteA = g_meterData.corrienteA;
    g_optDiag.corrienteB = g_meterData.corrienteB;
    g_optDiag.cosphi = g_meterData.cosphi;
    g_optDiag.frecuencia = g_meterData.frecuenciaMin;
    g_optDiag.energiaActiva = g_meterData.energiaActivaImp;
    g_optDiag.lecturaOk = readOk ? 1 : 0;

    if (!readOk) {
      debugPrintf("[ALERTA] Sonda óptica sin respuesta del medidor %s. Generando tramas de estado (Estado=2)...\n", g_reader.getMeterName());
      g_meterData.estado = 2; // Sin Lectura / Alerta IR
    } else {
      debugPrintf("[ÉXITO] Lectura óptica decodificada del medidor %s correctamente (Estado=0).\n", g_reader.getMeterName());
    }

    // 2. Empaquetar TRAMA 1 (Mensaje 0: Telemetría Principal) mediante BitPacker
    uint8_t payloadLen0 = BitPacker::packMessage0(g_meterData, g_binPayload0);

    debugPrintf("[PACKER] Trama 1 (Mensaje 0) empaquetada: %d bytes binarios -> ", payloadLen0);
    for (uint8_t i = 0; i < payloadLen0; i++) {
      if (g_binPayload0[i] < 0x10) debugPrint("0");
      debugPrintf("%X", g_binPayload0[i]);
    }
    debugPrintln();

    // 3. Empaquetar TRAMA 2 (Mensaje 1: Demandas / Energía Secundaria) mediante BitPacker
    uint8_t payloadLen1 = BitPacker::packMessage1(g_meterData, g_binPayload1);

    debugPrintf("[PACKER] Trama 2 (Mensaje 1) empaquetada: %d bytes binarios -> ", payloadLen1);
    for (uint8_t i = 0; i < payloadLen1; i++) {
      if (g_binPayload1[i] < 0x10) debugPrint("0");
      debugPrintf("%X", g_binPayload1[i]);
    }
    debugPrintln();

    // 4. Verificar estado de Red LoRaWAN y transmitir ambas tramas al servidor TTN
    if (loraHandler.isJoined()) {
      debugPrintln("[LORAWAN] ¡Conectado a TTN! Transmitiendo Tramas...");
      
      // LED 2 permanece encendido continuo durante el proceso de envío
      setLed(LED_2_PIN, true);

      debugPrintln("[LORAWAN] Transmitiendo Trama 1 (Mensaje 0)...");
      bool tx0 = loraHandler.sendPayload(g_binPayload0, payloadLen0, 10, false); // Trama 1 (Unconfirmed)

      if (tx0) {
        debugPrintln("[ÉXITO] Trama 1 (Mensaje 0) emitida al servidor TTN.");
      } else {
        debugPrintln("[ERROR] Falló la emisión de Trama 1.");
      }

      // Pausa entre tramas para cumplir con el ciclo de trabajo LoRa
      delay(2000);

      debugPrintln("[LORAWAN] Transmitiendo Trama 2 (Mensaje 1)...");
      bool tx1 = loraHandler.sendPayload(g_binPayload1, payloadLen1, 10, false); // Trama 2 (Unconfirmed)

      if (tx1) {
        debugPrintln("[ÉXITO] Trama 2 (Mensaje 1) emitida al servidor TTN.");
      } else {
        debugPrintln("[ERROR] Falló la emisión de Trama 2.");
      }

      // Apagar INMEDIATAMENTE el LED 2 al concluir el envío
      setLed(LED_2_PIN, false);

      if (tx0 || tx1) {
        setLed(LED_3_PIN, false);
        setLed(LED_4_PIN, false);
      } else {
        blinkLed(LED_3_PIN, 3, 100);
      }
    } else {
      debugPrintln("[ALERTA] Dispositivo aguardando conexión / reconexión a la red TTN.");
      setLed(LED_2_PIN, false);
      blinkLed(LED_3_PIN, 2, 100); // Parpadeo suave indicando espera de red
      loraHandler.joinOTAA(1000);  // Reintentar Join en segundo plano
    }
  }

  delay(100);
}
