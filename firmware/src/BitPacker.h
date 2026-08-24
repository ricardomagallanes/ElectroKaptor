#ifndef BIT_PACKER_H
#define BIT_PACKER_H

#include <Arduino.h>
#include "IMeterReader.h"

class BitPacker {
public:
  // Escribe 'numBits' del valor 'val' en el buffer comenzando en 'bitOffset'
  static void writeBits(uint8_t *buf, uint16_t &bitOffset, uint32_t val, uint8_t numBits);

  // Construye Mensaje 0: Telemetría Principal (devuelve la longitud en bytes)
  static uint8_t packMessage0(const MeterData &data, uint8_t *outBuffer);

  // Construye Mensaje 1: Configuración / Demandas / Energía Secundaria
  static uint8_t packMessage1(const MeterData &data, uint8_t *outBuffer);

  // Construye Mensaje 2: Facturación por Tarifas y Fases
  static uint8_t packMessage2(const MeterData &data, uint8_t *outBuffer);

  // Construye Mensaje 3: Calidad y Exportación por Fases
  static uint8_t packMessage3(const MeterData &data, uint8_t *outBuffer);

  // Convierte un buffer binario a su representación en caracteres ASCII Hex
  static uint8_t convertToAsciiHex(const uint8_t *inBuf, uint8_t inLen, uint8_t *outBuf);
};

#endif // BIT_PACKER_H
