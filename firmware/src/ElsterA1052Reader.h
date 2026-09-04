#ifndef ELSTER_A1052_READER_H
#define ELSTER_A1052_READER_H

#include "IMeterReader.h"

/**
 * @brief Lector Óptico para Medidor Trifásico Elster A1052 (Protocolo IEC 62056-21 Modo C a 300 baudios 7E1)
 * Hereda de BaseMeterReader para unificar la señalización LED y el flujo de control.
 */
class ElsterA1052Reader : public BaseMeterReader {
public:
  ElsterA1052Reader(uint8_t rxPin, uint8_t txPin);
  virtual ~ElsterA1052Reader() {}

  void begin(unsigned long baudRate = 300) override;
  const char* getMeterName() const override { return "Elster Trifásico A1052"; }

protected:
  bool performOpticalRead(MeterData &data, unsigned long timeoutMs) override;

private:
  void sendCharBitbang(char c, uint32_t bitTimeUs = 3333);
  void sendStringBitbang(const char* str, uint32_t bitTimeUs = 3333);
  int  readCharBitbang(unsigned long timeoutMs, uint32_t bitTimeUs = 3333);
};

#endif // ELSTER_A1052_READER_H
