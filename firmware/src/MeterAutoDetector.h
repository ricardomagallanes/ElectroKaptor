#ifndef METER_AUTO_DETECTOR_H
#define METER_AUTO_DETECTOR_H

#include <Arduino.h>
#include "MeterConfig.h"
#include "IMeterReader.h"

// Estructura de diagnóstico en RAM para inspección SWD/OpenOCD del proceso de auto-descubrimiento
struct __attribute__((packed)) AutoDetectDiag {
  uint32_t magic;           // 0xAD07EC70 ("AD-DETECT")
  uint8_t  detectedModel;   // 1..5 o 0 si no detectado
  uint8_t  detectionStage;  // 1: Pasivo 2400, 2: Activo 300, 3: Activo 2400, 4: Éxito, 5: Fallo
  uint8_t  attempts;        // Contador de intentos
  uint8_t  padding;
  char     modelName[32];   // Nombre legible del medidor detectado
  char     signature[64];   // Cadena o encabezado capturado durante la detección
};

extern volatile AutoDetectDiag g_autoDetectDiag;

/**
 * @brief Motor de Auto-Descubrimiento de Medidores Eléctricos en el Arranque
 * 
 * Escanea de forma secuencial y no destructiva el puerto óptico infrarrojo (PA3/PB10)
 * para identificar si el medidor conectado es:
 * 1. Elster A1052 (Trifásico - Emisión continua a 2400 baudios 7E1)
 * 2. Elster A150 (Monofásico - Emisión continua a 2400 baudios 8N1)
 * 3. Hexing HXE34K (Trifásico - IEC 62056-21 Modo C /HXE...)
 * 4. MIDDE DTS27 (Trifásico - IEC 62056-21 Modo C /DTS...)
 * 5. MIDDE DDS26D (Monofásico - IEC 62056-21 Modo 1 a 2400 baudios)
 */
class MeterAutoDetector {
public:
  /**
   * @brief Ejecuta el escaneo de detección en el arranque.
   * @param rxPin Pin RX del fototransistor infrarrojo (PA3)
   * @param txPin Pin TX del LED infrarrojo (PB10)
   * @param timeoutMs Tiempo máximo total permitido para la detección
   * @return Código del modelo detectado (METER_MODEL_*) o METER_MODEL_HEXING_HXE34K como fallback.
   */
  static uint8_t detectMeter(uint8_t rxPin, uint8_t txPin, unsigned long timeoutMs = 9000);

  /**
   * @brief Retorna una descripción legible del modelo detectado.
   */
  static const char* getModelName(uint8_t model);

  static int  readByteFast2400(uint8_t rxPin, uint32_t timeoutUs);
  static int  readCharBitbang7E1(uint8_t rxPin, unsigned long timeoutMs, uint32_t bitTimeUs);
  static int  readByteBitbang8N1(uint8_t rxPin, unsigned long timeoutMs, uint32_t bitTimeUs);
  static void sendCharBitbang7E1(uint8_t txPin, char c, uint32_t bitTimeUs);
  static void sendStringBitbang7E1(uint8_t txPin, const char* str, uint32_t bitTimeUs);
  static void sendOpticalWakeUp(uint8_t txPin, uint16_t durationMs);
};

#endif // METER_AUTO_DETECTOR_H
