#include <Arduino.h>
#include "BoardConfig.h"
#include "Credentials.h"
#include "IMeterReader.h"
#include "MiddeDTS27Reader.h"
#include "BitPacker.h"
#include "LoRaWAN_Handler.h"

// Instancia global de manejo LoRaWAN
LoRaWANHandler loraHandler;
HardwareSerial SerialDebug2(NC, PA2);  // Debug TX en PA2 (Libera PA3 para Puerto Óptico RX)

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
  pinMode(LED_LORA_PIN, OUTPUT);
  pinMode(LED_ERROR_PIN, OUTPUT);
  pinMode(LED_RESERVE, OUTPUT);
  pinMode(LED_MCU_PIN, OUTPUT);

  setLed(LED_LORA_PIN, false);
  setLed(LED_ERROR_PIN, false);
  setLed(LED_RESERVE, false);
  setLed(LED_MCU_PIN, false);
}

// Handlers de excepciones por seguridad
extern "C" __attribute__((used)) void HardFault_Handler(void) { NVIC_SystemReset(); }
extern "C" __attribute__((used)) void BusFault_Handler(void) { NVIC_SystemReset(); }
extern "C" __attribute__((used)) void UsageFault_Handler(void) { NVIC_SystemReset(); }

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
  SerialDebug2.begin(115200);
  initLeds();

  SerialDebug2.println("\r\n==================================================");
  SerialDebug2.println("===   ElectroKaptor ME_LoRa_v3.6 Firmware     ===");
  SerialDebug2.println("==================================================");

  // 1. Parpadeo inicial de prueba de LEDs
  blinkLed(LED_MCU_PIN, 2, 100);

  // 2. Inicializar módem LoRaWAN RAK3172 en PB6/PB7
  SerialDebug2.println("[INFO] Inicializando módem RAK3172 LoRaWAN...");
  if (!loraHandler.begin()) {
    SerialDebug2.println("[ERROR] Módem RAK3172 no responde. Verificando alimentacion/reset...");
    blinkLed(LED_ERROR_PIN, 5, 100);
  }

  // 3. Iniciar Unión OTAA a la Red TTN en Banda AU915 FSB2
  SerialDebug2.println("[INFO] Iniciando autenticación OTAA Join en AU915 FSB2...");
  
  bool joined = false;
  for (uint8_t attempt = 1; attempt <= 3; attempt++) {
    SerialDebug2.printf("[INFO] Intento #%d de OTAA Join...\n", attempt);
    setLed(LED_LORA_PIN, true);
    
    if (loraHandler.joinOTAA(15000)) {
      joined = true;
      break;
    }
    
    setLed(LED_LORA_PIN, false);
    blinkLed(LED_ERROR_PIN, 2, 200);
    delay(3000);
  }

  if (joined) {
    SerialDebug2.println("[EXITO] ¡¡¡Dispositivo conectado exitosamente a la Red LoRaWAN TTN!!!");
    setLed(LED_LORA_PIN, true);
    delay(1500);
    setLed(LED_LORA_PIN, false);
  } else {
    SerialDebug2.println("[ALERTA] No se pudo completar Join inmediato. Se reintentará en el loop principal.");
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

    SerialDebug2.printf("\r\n--- [CICLO LECTURA Y TELEMETRÍA #%lu] ---\n", (unsigned long)cycleCount);

    // 1. Lectura del Puerto Óptico (IEC 62056-21 / Medidor DTS27 en PA3 RX / PB10 TX)
    MeterData meterData;
    MiddeDTS27Reader reader(IR_RX_PIN, IR_TX_PIN);

    SerialDebug2.printf("[LECTURA] Intentando lectura de medidor óptico en PA3(RX / Pin 13) y PB10(TX / Pin 21)...\n");
    bool readOk = reader.readMeter(meterData, 4000);

    g_optDiag.magic = 0x0771C41D;
    g_optDiag.voltajeA = meterData.voltajeA;
    g_optDiag.voltajeB = meterData.voltajeB;
    g_optDiag.voltajeC = meterData.voltajeC;
    g_optDiag.corrienteA = meterData.corrienteA;
    g_optDiag.corrienteB = meterData.corrienteB;
    g_optDiag.cosphi = meterData.cosphi;
    g_optDiag.frecuencia = meterData.frecuenciaMin;
    g_optDiag.energiaActiva = meterData.energiaActivaImp;
    g_optDiag.lecturaOk = readOk ? 1 : 0;

    if (!readOk) {
      SerialDebug2.println("[ERROR LECTURA] No se recibió respuesta del medidor por la sonda óptica.");
      SerialDebug2.println("[ALERTA] Se cancela el envío LoRaWAN de este ciclo. Reintentando lectura en el próximo ciclo...");
      
      // Parpadeo de error (LED Rojo 2 - PB1)
      blinkLed(LED_ERROR_PIN, 4, 150);
      return; // Saltar transmisión hasta que la sonda esté conectada y la lectura sea válida
    }

    // Apagar LED de Error si la lectura fue exitosa
    setLed(LED_ERROR_PIN, false);

    SerialDebug2.println("[LECTURA] ¡Lectura óptica de medidor EXITOSA!");
    SerialDebug2.printf(" Voltaje A: %u V | Voltaje B: %u V | Voltaje C: %u V\n", meterData.voltajeA, meterData.voltajeB, meterData.voltajeC);
    SerialDebug2.printf(" Energía Activa Importada: %lu kWh*100\n", (unsigned long)meterData.energiaActivaImp);

    // 2. Empaquetar datos reales del medidor en Mensaje 0 (Telemetría Principal) mediante BitPacker
    uint8_t binPayload[32];
    uint8_t payloadLen = BitPacker::packMessage0(meterData, binPayload);

    SerialDebug2.printf("[PACKER] Telemetría empaquetada: %d bytes binarios -> ", payloadLen);
    for (uint8_t i = 0; i < payloadLen; i++) {
      if (binPayload[i] < 0x10) SerialDebug2.print("0");
      SerialDebug2.print(binPayload[i], HEX);
    }
    SerialDebug2.println();

    // 3. Transmisión LoRaWAN vía módem RAK3172
    SerialDebug2.println("[LORAWAN] Transmitiendo paquete de telemetría por radio...");
    setLed(LED_LORA_PIN, true); // Enciende LED LoRa (PB0) durante transmisión

    bool txSuccess = loraHandler.sendPayload(binPayload, payloadLen, 10);
    setLed(LED_LORA_PIN, false);

    if (txSuccess) {
      SerialDebug2.println("[EXITO] Paquete transmitido y confirmado por LoRaWAN.");
    } else {
      SerialDebug2.println("[ERROR] Fallo en la transmisión por LoRaWAN.");
      blinkLed(LED_ERROR_PIN, 3, 150);
    }
  }

  delay(100);
}
