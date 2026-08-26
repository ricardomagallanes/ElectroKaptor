#include <Arduino.h>
#include "BoardConfig.h"
#include "MeterConfig.h"
#include "MeterReaderFactory.h"
#include "LoRaWAN_Handler.h"
#include "BitPacker.h"

extern "C" __attribute__((used)) void HardFault_Handler(void) { NVIC_SystemReset(); }
extern "C" __attribute__((used)) void BusFault_Handler(void) { NVIC_SystemReset(); }
extern "C" __attribute__((used)) void UsageFault_Handler(void) { NVIC_SystemReset(); }
extern "C" __attribute__((used)) void ADC1_2_IRQHandler(void) {}

static IMeterReader* g_meterReader = nullptr;
static LoRaWANHandler g_loraHandler;

void setup() {
  volatile void* dummy1 = (void*)&ADC1_2_IRQHandler;
  volatile void* dummy2 = (void*)&HardFault_Handler;
  (void)dummy1; (void)dummy2;

  // Habilitar DWT CYCCNT para microsegundos exactos
  CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
  DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;

  // Inicializar LEDs de estado
  pinMode(LED_LORA_PIN, OUTPUT);  digitalWrite(LED_LORA_PIN, LOW);
  pinMode(LED_ERROR_PIN, OUTPUT); digitalWrite(LED_ERROR_PIN, LOW);
  pinMode(LED_MCU_PIN, OUTPUT);   digitalWrite(LED_MCU_PIN, HIGH);

  // Crear Lector de Medidor Óptico DTS27 (PA9 TX / PA10 RX)
  g_meterReader = MeterReaderFactory::createMeterReader(IR_RX_PIN, IR_TX_PIN);
  if (g_meterReader) {
    g_meterReader->begin(IR_DEFAULT_BAUD_RATE);
  }

  // Inicializar Módem LoRaWAN RAK3172 (PB6 TX / PB7 RX, PA1 Reset)
  if (g_loraHandler.begin()) {
    if (g_loraHandler.joinOTAA(25000)) {
      digitalWrite(LED_LORA_PIN, HIGH); // Encender LED LoRa al conectar
    } else {
      digitalWrite(LED_ERROR_PIN, HIGH);
    }
  } else {
    digitalWrite(LED_ERROR_PIN, HIGH);
  }
}

void loop() {
  // Parpadeo LED Status MCU
  digitalWrite(LED_MCU_PIN, LOW);  delay(100);
  digitalWrite(LED_MCU_PIN, HIGH); delay(100);

  // Reintento automático de Join si se desconecta
  if (!g_loraHandler.isJoined()) {
    digitalWrite(LED_LORA_PIN, LOW);
    if (g_loraHandler.joinOTAA(20000)) {
      digitalWrite(LED_LORA_PIN, HIGH);
      digitalWrite(LED_ERROR_PIN, LOW);
    }
  }

  // Lectura periódica del medidor óptico cada 30 segundos
  static unsigned long lastRead = 0;
  if (millis() - lastRead > 30000 || lastRead == 0) {
    lastRead = millis();

    MeterData data;
    if (g_meterReader && g_meterReader->readMeter(data)) {
      uint8_t payload[32];
      uint8_t len = 0;

#if SELECTED_PAYLOAD_FORMAT == PAYLOAD_FORMAT_ASCII_HEX
      char asciiBuf[32];
      snprintf(asciiBuf, sizeof(asciiBuf), "%010lu%010lu", data.energiaActivaImp, data.energiaReactivaImp);
      len = strlen(asciiBuf);
      memcpy(payload, asciiBuf, len);
#else
      len = BitPacker::packMeterData(data, payload, sizeof(payload));
#endif

      if (len > 0 && g_loraHandler.isJoined()) {
        g_loraHandler.sendPayload(payload, len, 10);
      }
    }
  }

  g_loraHandler.process(100);
}
