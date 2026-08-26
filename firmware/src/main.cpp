#include <Arduino.h>
#include "BoardConfig.h"
#include "MeterConfig.h"
#include "IMeterReader.h"
#include "MeterReaderFactory.h"
#include "BitPacker.h"
#include "LoRaWAN_Handler.h"
#include "Credentials.h"

// Manejadores de excepciones para evitar bloqueos por faltas de bus o interrupciones
extern "C" __attribute__((used)) void HardFault_Handler(void) { NVIC_SystemReset(); }
extern "C" __attribute__((used)) void BusFault_Handler(void) { NVIC_SystemReset(); }
extern "C" __attribute__((used)) void UsageFault_Handler(void) { NVIC_SystemReset(); }
extern "C" __attribute__((used)) void ADC1_2_IRQHandler(void) {}

IMeterReader* g_meterReader = nullptr;
LoRaWANHandler g_loraHandler;

void setup() {
  volatile void* dummy1 = (void*)&ADC1_2_IRQHandler;
  volatile void* dummy2 = (void*)&HardFault_Handler;
  (void)dummy1; (void)dummy2;

  Serial.begin(SERIAL_DEBUG_BAUD);
  delay(1000);

  Serial.println("\n==================================================");
  Serial.println("   ELECTROKAPTOR - FIRMWARE INTEGRADO UNIFICADO   ");
  Serial.printf("   Placa: %s\n", BOARD_NAME);
  Serial.printf("   Medidor Activo: MIDDE DTS27 (IEC 62056-21)\n");
  Serial.printf("   Pines Módulo Óptico: RX=PA10, TX=PA9 (USART1)\n");
  Serial.println("==================================================\n");

#if SELECTED_BOARD_MODEL == BOARD_STM32F103C8T6
  pinMode(LED_FAIL_PIN, OUTPUT);
  pinMode(LED_TX_LORA_PIN, OUTPUT);
  pinMode(LED_UNUSED_PIN, OUTPUT);
  pinMode(POWER_SENSE_PIN, INPUT);

  // Parpadeo inicial de leds de diagnóstico
  digitalWrite(LED_FAIL_PIN, LOW);    delay(100); digitalWrite(LED_FAIL_PIN, HIGH);
  digitalWrite(LED_TX_LORA_PIN, LOW); delay(100); digitalWrite(LED_TX_LORA_PIN, HIGH);
  digitalWrite(LED_UNUSED_PIN, LOW);  delay(100); digitalWrite(LED_UNUSED_PIN, HIGH);
#endif

  // Instanciar el lector de medidor configurado (Midde DTS27) a través de la fábrica
  g_meterReader = MeterReaderFactory::createMeterReader(IR_RX_PIN, IR_TX_PIN);
  if (g_meterReader) {
    g_meterReader->begin(IR_DEFAULT_BAUD_RATE);
    Serial.printf("[MAIN] Lector de medidor '%s' inicializado con éxito.\n", g_meterReader->getMeterName());
  } else {
    Serial.println("[MAIN] Error crítico: No se pudo instanciar el lector de medidor.");
  }

  // Inicializar radio LoRaWAN
  g_loraHandler.begin();
}

void loop() {
  Serial.println("\n--------------------------------------------------");
  Serial.println("[MAIN] Iniciando ciclo de telemetría y lectura...");

#if SELECTED_BOARD_MODEL == BOARD_STM32F103C8T6
  digitalWrite(LED_TX_LORA_PIN, LOW); // LED activo durante la lectura
#endif

  MeterData data;
  memset(&data, 0, sizeof(MeterData));

  bool success = false;
  if (g_meterReader) {
    success = g_meterReader->readMeter(data, IR_DEFAULT_TIMEOUT_MS);
  }

#if SELECTED_BOARD_MODEL == BOARD_STM32F103C8T6
  digitalWrite(LED_TX_LORA_PIN, HIGH); // Apagar indicador de lectura IR
#endif

  if (success) {
    Serial.println("[MAIN] ¡Lectura del medidor completada con éxito!");

#if SELECTED_BOARD_MODEL == BOARD_STM32F103C8T6
    digitalWrite(LED_FAIL_PIN, HIGH); // LED FAIL apagado (Normal)
#endif

    // Empaquetar la telemetría en el buffer binario
    uint8_t rawPayload[16];
    memset(rawPayload, 0, sizeof(rawPayload));
    uint8_t payloadLen = BitPacker::packMessage0(data, rawPayload);

    Serial.printf("[MAIN] Payload binario generado (%d bytes): ", payloadLen);
    for (uint8_t i = 0; i < payloadLen; i++) {
      Serial.printf("%02X ", rawPayload[i]);
    }
    Serial.println();

#if SELECTED_PAYLOAD_FORMAT == PAYLOAD_FORMAT_ASCII_HEX
    uint8_t asciiPayload[32];
    memset(asciiPayload, 0, sizeof(asciiPayload));
    uint8_t asciiLen = BitPacker::convertToAsciiHex(rawPayload, payloadLen, asciiPayload);
    Serial.printf("[MAIN] Payload ASCII-HEX (%d bytes): %s\n", asciiLen, (char*)asciiPayload);
    g_loraHandler.sendPayload(asciiPayload, asciiLen, 10);
#else
    g_loraHandler.sendPayload(rawPayload, payloadLen, 10);
#endif

  } else {
    Serial.println("[MAIN] Error o timeout en la lectura del medidor.");

#if SELECTED_BOARD_MODEL == BOARD_STM32F103C8T6
    digitalWrite(LED_FAIL_PIN, LOW); // Indicar fallo mediante LED FAIL
#endif
  }

  Serial.printf("[MAIN] Esperando %d ms para el próximo ciclo...\n", TELEMETRY_INTERVAL_MS);
  delay(TELEMETRY_INTERVAL_MS);
}
