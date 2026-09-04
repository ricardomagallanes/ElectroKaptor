#ifndef HEXING_HXE34K_READER_H
#define HEXING_HXE34K_READER_H

#include <Arduino.h>
#include "IMeterReader.h"

/**
 * @brief Lector Óptico para Medidor Trifásico Hexing Modelo HXE34K
 * 
 * Protocolo: IEC 62056-21 Modo C a 300 baudios 7E1 con secuencia previa de wake-up óptico.
 * Decodifica registros OBIS estándar para medición trifásica de energía activa, reactiva,
 * tensiones por fase (32.7.0, 52.7.0, 72.7.0), corrientes (31.7.0, 51.7.0, 71.7.0),
 * factor de potencia y máxima demanda.
 */
class HexingHXE34KReader : public BaseMeterReader {
public:
  HexingHXE34KReader(uint8_t rxPin, uint8_t txPin);
  virtual ~HexingHXE34KReader() {}

  void begin(unsigned long baudRate = 300) override;
  const char* getMeterName() const override { return "Hexing HXE34K (Trifasico)"; }

protected:
  bool performOpticalRead(MeterData &data, unsigned long timeoutMs) override;

private:
  int  readCharBitbang(unsigned long timeoutMs, uint32_t bitTimeUs = 3333);
  void sendCharBitbang(char c, uint32_t bitTimeUs = 3333);
  void sendStringBitbang(const char* str, uint32_t bitTimeUs = 3333);
  void sendWakeUpOptical(uint16_t durationMs = 250);
  int  readByteFast2400(uint32_t timeoutUs);
};

#endif // HEXING_HXE34K_READER_H
