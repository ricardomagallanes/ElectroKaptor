#ifndef METER_CONFIG_H
#define METER_CONFIG_H

#include "BoardConfig.h"

// Modelos de medidores soportados
#define METER_MODEL_AUTO_DETECT   0  // Detección automática dinámica en arranque (Auto-Discovery)
#define METER_MODEL_ELSTER_A150   1  // Elster Monofásico A150 (2400 baudios 8N1 continuo)
#define METER_MODEL_MIDDE_DTS27   2  // MIDDE Trifásico DTS27 (300 baudios 7E1 IEC 62056-21 Modo C)
#define METER_MODEL_MIDDE_DDS26D  3  // MIDDE Monofásico DDS26D (2400 baudios 7E1 IEC 62056-21 Modo 1)
#define METER_MODEL_ELSTER_A1052  4  // Elster Trifásico A1052 (2400 baudios 7E1 continuo OBIS)
#define METER_MODEL_HEXING_HXE34K 5  // Hexing Trifásico HXE34K (300/4800 baudios 7E1 IEC 62056-21 Modo C)

// Formatos de payload para transmisión por LoRaWAN
#define PAYLOAD_FORMAT_RAW_BINARY  1  // 15 bytes binarios (más compacto)
#define PAYLOAD_FORMAT_ASCII_HEX   2  // 30 bytes ASCII Hex (réplica exacta 1:1 del equipo funcionando)

// =========================================================================
// SELECCIÓN DEL MEDIDOR ACTIVO Y FORMATO DE TRANSMISIÓN
// =========================================================================
// MODO AUTO-DESCUBRIMIENTO (Predeterminado):
// El firmware identifica automáticamente en el arranque cuál medidor está conectado,
// bloquea el driver para toda la sesión y opera con él hasta el próximo apagado/reinicio.
#define SELECTED_METER_MODEL    METER_MODEL_AUTO_DETECT
// #define SELECTED_METER_MODEL    METER_MODEL_ELSTER_A1052
// #define SELECTED_METER_MODEL    METER_MODEL_HEXING_HXE34K
// #define SELECTED_METER_MODEL    METER_MODEL_MIDDE_DTS27
// #define SELECTED_METER_MODEL    METER_MODEL_MIDDE_DDS26D
// #define SELECTED_METER_MODEL    METER_MODEL_ELSTER_A150

#define SELECTED_PAYLOAD_FORMAT PAYLOAD_FORMAT_ASCII_HEX

// Configuración de comunicación infrarroja (DTS27 protocolo IEC 62056-21 Modo C a 300 baudios 7E1)
#define IR_DEFAULT_BAUD_RATE  300
#define IR_DEFAULT_TIMEOUT_MS 10000

#endif // METER_CONFIG_H
