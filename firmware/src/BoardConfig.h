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
 * 4. CONTROL DE RELAYS (CORTE Y RECONEXIÓN REMOTA DE LUZ):
 *    - RELAY_DISCONNECT_PIN : PA8 (Pin 29 del STM32 - Relay Desconexión)
 *    - RELAY_CONNECT_PIN    : PA9 (Pin 30 del STM32 - Relay Conexión)
 * 
 * ============================================================================
 */

// ----------------------------------------------------------------------------
// 1. ASIGNACIÓN OFICIAL DE PINES - PUERTO ÓPTICO (VERIFICADO EN HARDWARE)
// ----------------------------------------------------------------------------
#define IR_TX_PIN   PB10 // Pin 21 del STM32 LQFP48 (Puerto Óptico TX)
#define IR_RX_PIN   PA3  // Pin 13 del STM32 LQFP48 (Puerto Óptico RX)

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
// 2. ASIGNACIÓN OFICIAL DE PINES - MÓDULO LORAWAN RAK3172 (VERIFICADO Y PROBADO)
// ----------------------------------------------------------------------------
#define LORA_TX_PIN   PB6     // Pin 42 del STM32 (USART1_TX Remapped -> RAK Pin 1 UART2_RX)
#define LORA_RX_PIN   PB7     // Pin 43 del STM32 (USART1_RX Remapped -> RAK Pin 2 UART2_TX)
#define LORA_RST_PIN  PB8     // Pin 45 del STM32 (Control NRST -> RAK Pin 22 NRST)
#define LORA_BAUD     115200  // Baudrate oficial RUI3 v4.0.6
#define LORA_BAND     6       // Banda AU915 para TTN / Argentina
#define LORA_MASK     0x0002  // Sub-banda 2 / FSB2 (Canales 8-15)

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

// ----------------------------------------------------------------------------
// 4. ASIGNACIÓN DE PINES - CONTROL DE RELAYS (CONEXIÓN Y DESCONEXIÓN REMOTA)
// ----------------------------------------------------------------------------
#define RELAY_DISCONNECT_PIN  PA8 // Pin 29 del STM32 LQFP48 (Control Relay Desconexión de Luz)
#define RELAY_CONNECT_PIN     PA9 // Pin 30 del STM32 LQFP48 (Control Relay Conexión de Luz)

#endif // BOARD_CONFIG_H
