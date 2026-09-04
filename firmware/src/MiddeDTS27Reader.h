#ifndef MIDDE_DTS27_READER_H
#define MIDDE_DTS27_READER_H

#include <Arduino.h>
#include "IMeterReader.h"

class MiddeDTS27Reader : public BaseMeterReader {
public:
  MiddeDTS27Reader(uint8_t rxPin, uint8_t txPin);
  virtual ~MiddeDTS27Reader() {}

  void begin(unsigned long baudRate = 300) override;
  const char* getMeterName() const override { return "Midde Trifasico DTS27"; }

protected:
  bool performOpticalRead(MeterData &data, unsigned long timeoutMs) override;

private:
  int readCharBitbang(unsigned long timeoutMs);
  void sendCharBitbang(char c);
  void sendStringBitbang(const char* str);
};

#endif // MIDDE_DTS27_READER_H
