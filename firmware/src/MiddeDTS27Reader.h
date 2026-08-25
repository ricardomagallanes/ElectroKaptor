#ifndef MIDDE_DTS27_READER_H
#define MIDDE_DTS27_READER_H

#include <Arduino.h>
#include "IMeterReader.h"

class MiddeDTS27Reader : public IMeterReader {
public:
  MiddeDTS27Reader(uint8_t rxPin, uint8_t txPin);
  virtual ~MiddeDTS27Reader() {}

  void begin(unsigned long baudRate = 2400) override;
  bool readMeter(MeterData &data, unsigned long timeoutMs = 10000) override;
  const char* getMeterName() const override { return "Midde Trifasico DTS27"; }

private:
  uint8_t _rxPin;
  uint8_t _txPin;
#if defined(ARDUINO_ARCH_STM32)
  #define _irSerial Serial1
#else
  HardwareSerial _irSerial;
#endif

  bool detectSyncSequence(unsigned long timeoutMs);
  uint8_t mapByteToNibble(uint8_t val);
};

#endif // MIDDE_DTS27_READER_H
