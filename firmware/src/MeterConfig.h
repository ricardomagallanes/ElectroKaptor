#ifndef METER_CONFIG_H
#define METER_CONFIG_H

#include "BoardConfig.h"

// Modelos de medidores soportados
#define METER_MODEL_ELSTER_A150   1
#define METER_MODEL_MIDDE_DTS27   2
#define METER_MODEL_MIDDE_DDS26D  3
#define METER_MODEL_ELSTER_A1052  4
#define METER_MODEL_HEXING_HXE34K 5

// Formatos de payload para transmisión por LoRaWAN
#define PAYLOAD_FORMAT_RAW_BINARY  1  // 15 bytes binarios (más compacto)
#define PAYLOAD_FORMAT_ASCII_HEX   2  // 30 bytes ASCII Hex (réplica exacta 1:1 del equipo funcionando)

// =========================================================================
// SELECCIÓN DEL MEDIDOR ACTIVO Y FORMATO DE TRANSMISIÓN
// =========================================================================
#define SELECTED_METER_MODEL    METER_MODEL_HEXING_HXE34K
#define SELECTED_PAYLOAD_FORMAT PAYLOAD_FORMAT_ASCII_HEX

// Configuración de comunicación infrarroja (DTS27 protocolo IEC 62056-21 Modo C a 300 baudios 7E1)
#define IR_DEFAULT_BAUD_RATE  300
#define IR_DEFAULT_TIMEOUT_MS 10000

#endif // METER_CONFIG_H
