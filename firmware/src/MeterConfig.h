#ifndef METER_CONFIG_H
#define METER_CONFIG_H

#include "BoardConfig.h"

// Modelos de medidores soportados
#define METER_MODEL_ELSTER_A150  1
#define METER_MODEL_MIDDE_DTS27  2

// Formatos de payload para transmisión por LoRaWAN
#define PAYLOAD_FORMAT_RAW_BINARY  1  // 15 bytes binarios (más compacto)
#define PAYLOAD_FORMAT_ASCII_HEX   2  // 30 bytes ASCII Hex (réplica exacta 1:1 del equipo funcionando)

// =========================================================================
// SELECCIÓN DEL MEDIDOR ACTIVO Y FORMATO DE TRANSMISIÓN
// =========================================================================
// PLACA ACTIVA: Heltec WiFi LoRa 32 V3 (ESP32-S3 con Pantalla OLED)
#undef SELECTED_BOARD_MODEL
#define SELECTED_BOARD_MODEL    BOARD_HELTEC_ESP32_S3

#define SELECTED_METER_MODEL    METER_MODEL_MIDDE_DTS27
#define SELECTED_PAYLOAD_FORMAT PAYLOAD_FORMAT_ASCII_HEX

// Configuración de comunicación infrarroja (DTS27 velocidad 9600 baudios)
#define IR_DEFAULT_BAUD_RATE  9600
#define IR_DEFAULT_TIMEOUT_MS 10000

#endif // METER_CONFIG_H
