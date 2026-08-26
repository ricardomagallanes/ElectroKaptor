#ifndef BOARD_CONFIG_H
#define BOARD_CONFIG_H

#include <Arduino.h>

#define BOARD_HELTEC_ESP32_S3  1
#define BOARD_STM32F103C8T6    2

/**
 * ============================================================================
 * @file BoardConfig.h
 * @brief Configuración de Hardware de la Placa Principal ME_LoRa_v3.6 (STM32F103C8T6)
 * ============================================================================
 * 
 * MAPA FINAL DE PINES VERIFICADO EN HARDWARE:
 * 
 * 1. PUERTO ÓPTICO (IEC 62056-21 Modo C / Medidor DTS27):
 *    - IR_TX_PIN : PA9  (USART1 TX Nativo)
 *    - IR_RX_PIN : PA10 (USART1 RX Nativo)
 *    - Baudrate  : 300 baudios, 7E1 (Modo C Handshake)
 * 
 * 2. MÓDULO LORAWAN (RAK3172 AT Modem):
 *    - LORA_TX_PIN  : PB6 (USART1 TX Remappeado)
 *    - LORA_RX_PIN  : PB7 (USART1 RX Remappeado)
 *    - LORA_RST_PIN : PA1 (Pin Reset NRST Activo en LOW)
 *    - Baudrate     : 115200 baudios (Band 6 / AU915)
 * 
 * 3. LEDS DE ESTADO Y MONITOREO:
 *    - LED_LORA_PIN  : PB0  (LED 3 - Estado de Red LoRaWAN)
 *    - LED_ERROR_PIN : PB1  (LED 2 - Fallo / Error)
 *    - LED_RESERVE   : PB2  (LED 4 - Reserva)
 *    - LED_MCU_PIN   : PC13 (LED Onboard STM32)
 * 
 * ============================================================================
 */

// ----------------------------------------------------------------------------
// 1. ASIGNACIÓN OFICIAL DE PINES - PUERTO ÓPTICO (VERIFICADO)
// ----------------------------------------------------------------------------
#define IR_TX_PIN   PA9  // Puerto Óptico TX
#define IR_RX_PIN   PA10 // Puerto Óptico RX

/*
 * PINES AUDITADOS Y DESCARTADOS PARA PUERTO ÓPTICO:
 * // #define IR_TX_PIN_CANDIDATE_1 PB10 // Descartado
 * // #define IR_RX_PIN_CANDIDATE_1 PB11 // Descartado
 * // #define IR_TX_PIN_CANDIDATE_2 PB6  // Asignado a Módem LoRaWAN
 * // #define IR_RX_PIN_CANDIDATE_2 PB7  // Asignado a Módem LoRaWAN
 * // #define IR_TX_PIN_CANDIDATE_3 PA2  // Descartado
 * // #define IR_RX_PIN_CANDIDATE_3 PA3  // Descartado
 * // #define IR_TX_PIN_CANDIDATE_4 PB12 // Descartado
 * // #define IR_RX_PIN_CANDIDATE_4 PB13 // Descartado
 */

// ----------------------------------------------------------------------------
// 2. ASIGNACIÓN OFICIAL DE PINES - MÓDULO LORAWAN RAK3172 (VERIFICADO)
// ----------------------------------------------------------------------------
#define LORA_TX_PIN   PB6  // Módem LoRaWAN TX (USART1 Remapped)
#define LORA_RX_PIN   PB7  // Módem LoRaWAN RX (USART1 Remapped)
#define LORA_RST_PIN  PA1  // Módem LoRaWAN Reset NRST

/*
 * PINES AUDITADOS Y DESCARTADOS PARA MÓDULO LORAWAN:
 * // #define LORA_TX_CANDIDATE_1 PA2  // USART2 TX (Descartado)
 * // #define LORA_RX_CANDIDATE_1 PA3  // USART2 RX (Descartado)
 * // #define LORA_TX_CANDIDATE_2 PB10 // USART3 TX (Descartado)
 * // #define LORA_RX_CANDIDATE_2 PB11 // USART3 RX (Descartado)
 * // #define LORA_SPI_NSS_1      PA4  // SPI1 NSS (Descartado)
 * // #define LORA_SPI_SCK_1      PA5  // SPI1 SCK (Descartado)
 * // #define LORA_SPI_MISO_1     PA6  // SPI1 MISO (Descartado)
 * // #define LORA_SPI_MOSI_1     PA7  // SPI1 MOSI (Descartado)
 * // #define LORA_SPI_NSS_2      PB12 // SPI2 NSS (Descartado)
 * // #define LORA_SPI_SCK_2      PB13 // SPI2 SCK (Descartado)
 * // #define LORA_SPI_MISO_2     PB14 // SPI2 MISO (Descartado)
 * // #define LORA_SPI_MOSI_2     PB15 // SPI2 MOSI (Descartado)
 */

// ----------------------------------------------------------------------------
// 3. ASIGNACIÓN DE PINES - LEDS E INDICADORES DE ESTADO (VERIFICADO)
// ----------------------------------------------------------------------------
#define LED_LORA_PIN   PB0  // LED 3 - Estado LoRa
#define LED_ERROR_PIN  PB1  // LED 2 - Error
#define LED_RESERVE    PB2  // LED 4 - Reserva
#define LED_MCU_PIN    PC13 // LED Onboard Status

#endif // BOARD_CONFIG_H
