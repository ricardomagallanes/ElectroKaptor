#ifndef ELSTER_A150_READER_H
#define ELSTER_A150_READER_H

#include <Arduino.h>
#include "IMeterReader.h"

class ElsterA150Reader : public IMeterReader {
public:
  ElsterA150Reader(uint8_t rxPin, uint8_t txPin);
  virtual ~ElsterA150Reader() {}

  void begin(unsigned long baudRate = 2400) override;
  bool readMeter(MeterData &data, unsigned long timeoutMs = 10000) override;
  const char* getMeterName() const override { return "Elster A150"; }

private:
  uint8_t _rxPin;
  uint8_t _txPin;
  HardwareSerial _irSerial;

  bool detectSyncSequence(unsigned long timeoutMs);
  uint8_t mapByteToNibble(uint8_t val);
};

#endif // ELSTER_A150_READER_H
