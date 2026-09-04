#ifndef ELSTER_A150_READER_H
#define ELSTER_A150_READER_H

#include <Arduino.h>
#include "IMeterReader.h"

/**
 * @brief Driver para lectura óptica de medidores monofásicos Elster A150
 * Protocolo infrarrojo a 2400 baudios 8N1 con sincronismo de 8 bytes y paquetes de 186 bytes.
 */
class ElsterA150Reader : public BaseMeterReader {
public:
  ElsterA150Reader(uint8_t rxPin, uint8_t txPin);
  virtual ~ElsterA150Reader() {}

  void begin(unsigned long baudRate = 2400) override;
  const char* getMeterName() const override { return "Elster A150 (Monofasico)"; }

protected:
  bool performOpticalRead(MeterData &data, unsigned long timeoutMs) override;

private:
  int readByteFast(uint32_t timeoutUs);
  uint8_t mapByteToNibble(uint8_t val);
};

#endif // ELSTER_A150_READER_H
