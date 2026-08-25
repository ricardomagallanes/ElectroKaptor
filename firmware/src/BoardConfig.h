#ifndef BOARD_CONFIG_H
#define BOARD_CONFIG_H

#include <Arduino.h>

// Modelos de placas de desarrollo soportadas
#define BOARD_HELTEC_ESP32_S3  1
#define BOARD_STM32F103C8T6    2

// =========================================================================
// SELECCIÓN DE LA PLACA DE DESARROLLO ACTIVA (Auto-detectada o Manual)
// =========================================================================
#if defined(STM32F103xB) || defined(ARDUINO_ARCH_STM32)
  #define SELECTED_BOARD_MODEL BOARD_STM32F103C8T6
#else
  #define SELECTED_BOARD_MODEL BOARD_HELTEC_ESP32_S3
#endif

// =========================================================================
// CONFIGURACIÓN DE PINES Y PERIFÉRICOS SEGÚN LA PLACA SELECCIONADA
// =========================================================================

#undef BOARD_NAME
#if SELECTED_BOARD_MODEL == BOARD_HELTEC_ESP32_S3

  #define BOARD_NAME "Heltec ESP32-S3 (WiFi LoRa 32 V3)"
  
  // Pines de Pantalla OLED SSD1306 (0.96 pulg I2C) en Heltec V3
  #define HAS_OLED_DISPLAY 1
  #define OLED_SDA_PIN   17  // I2C SDA para OLED
  #define OLED_SCL_PIN   18  // I2C SCL para OLED
  #define OLED_RST_PIN   21  // Reset del display
  #define OLED_VEXT_PIN  36  // Control de alimentación Vext del display (LOW activa)

  // Puerto serie Infrarrojo en Heltec V3 (Pin impreso 4=RX, Pin impreso 5=TX)
  #define IR_RX_PIN 4
  #define IR_TX_PIN 5

  // Pines de Radio LoRaWAN SX1262 nativa en Heltec V3
  #define LORA_NSS_PIN   8
  #define LORA_DIO1_PIN  14
  #define LORA_RST_PIN   12
  #define LORA_BUSY_PIN  13

  #ifndef TELEMETRY_INTERVAL_MS
    #define TELEMETRY_INTERVAL_MS (15 * 1000) // 15 segundos para pruebas
  #endif

  #define SERIAL_DEBUG_BAUD 115200

#elif SELECTED_BOARD_MODEL == BOARD_STM32F103C8T6

  #define BOARD_NAME "STM32F103C8T6 (Blue Pill)"

  // Puerto serie UART Infrarrojo (Serial1: PA10=RX, PA9=TX)
  #define IR_RX_PIN PA10
  #define IR_TX_PIN PA9

  // Pines de Radio LoRaWAN SPI (Módulo externo RFM95W / SX1276 / SX1262)
  #define LORA_NSS_PIN   PA4
  #define LORA_SCK_PIN   PA5
  #define LORA_MISO_PIN  PA6
  #define LORA_MOSI_PIN  PA7
  #define LORA_RST_PIN   PB1
  #define LORA_DIO0_PIN  PB0

  // Pines de LEDs de Estado Gabinete Kaptor
  #define LED_POWER_PIN   PB12  // D6 Verde: Power (Alimentación)
  #define LED_FAIL_PIN    PB13  // D5 Rojo: Fail (Falla de comunicación)
  #define LED_TX_LORA_PIN PB14  // D3 Rojo: Tx LoRa (Transmisión)
  #define LED_UNUSED_PIN  PB15  // D4 Rojo: Reserva (Sin uso)

  // Pin de detección de alimentación externa 220V (Fuente Hi-Link)
  #define POWER_SENSE_PIN PA1   // Sensa presencia de energía de red externa

  // Intervalo entre lecturas de telemetría (en milisegundos)
  #ifndef TELEMETRY_INTERVAL_MS
    #define TELEMETRY_INTERVAL_MS (15 * 1000) // 15 segundos para pruebas
  #endif

  #define SERIAL_DEBUG_BAUD 115200

#else
  #error "Modelo de placa no soportado o no seleccionado en BoardConfig.h"
#endif

#endif // BOARD_CONFIG_H
