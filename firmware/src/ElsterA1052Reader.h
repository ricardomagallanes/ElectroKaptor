#ifndef ELSTER_A1052_READER_H
#define ELSTER_A1052_READER_H

#include "IMeterReader.h"

/**
 * @brief Lector Óptico para Medidor Trifásico Elster A1052
 * Soporta modo Dual: Ráfaga continua 2400 baudios 8N1 e interrogación IEC 62056-21 Modo C a 300 baudios 7E1.
 */
class ElsterA1052Reader : public BaseMeterReader {
public:
  ElsterA1052Reader(uint8_t rxPin, uint8_t txPin);
  virtual ~ElsterA1052Reader() {}

  void begin(unsigned long baudRate = 2400) override;
  const char* getMeterName() const override { return "Elster Trifásico A1052"; }

protected:
  bool performOpticalRead(MeterData &data, unsigned long timeoutMs) override;

private:
  int  readByteFast2400(uint32_t timeoutUs);
  void sendCharBitbang(char c, uint32_t bitTimeUs = 3333);
  void sendStringBitbang(const char* str, uint32_t bitTimeUs = 3333);
  int  readCharBitbang(unsigned long timeoutMs, uint32_t bitTimeUs = 3333);
};

#endif // ELSTER_A1052_READER_H
