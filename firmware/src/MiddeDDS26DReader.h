#ifndef MIDDE_DDS26D_READER_H
#define MIDDE_DDS26D_READER_H

#include <Arduino.h>
#include "IMeterReader.h"

/**
 * @brief Driver para lectura óptica de medidores monofásicos MIDDE DDS26D
 * Soporta escaneo y autodetección de protocolos:
 * - IEC 62056-21 Modo C con Wake-up óptico sostenido (300 / 2400 / 9600 baud)
 * - DLMS / COSEM sobre enlace HDLC a 9600 baud
 * - Modo espontáneo continuo
 */
class MiddeDDS26DReader : public BaseMeterReader {
public:
  MiddeDDS26DReader(uint8_t rxPin, uint8_t txPin);
  virtual ~MiddeDDS26DReader() {}

  void begin(unsigned long baudRate = 2400) override;
  const char* getMeterName() const override { return "MIDDE DDS26D (Monofasico)"; }

protected:
  bool performOpticalRead(MeterData &data, unsigned long timeoutMs) override;

private:
  void sendWakeupPulse(uint16_t durationMs);
  void sendChar(char c, uint32_t bitTimeUs, bool is7E1);
  void sendString(const char* str, uint32_t bitTimeUs, bool is7E1);
  void sendBytes(const uint8_t* buf, size_t len, uint32_t bitTimeUs);
  void sendIecBlock(char cmd, char param, const char* body, uint32_t bitTimeUs);
  bool readRegister(const char* obisCode, String &outVal, uint32_t bitTimeUs);
  int  readChar(uint32_t bitTimeUs, bool is7E1, unsigned long timeoutMs);
  int  readRawByte(uint32_t bitTimeUs, unsigned long timeoutMs);
  
  bool tryIecSignon(uint32_t baud, bool withWakeup, String &outId);
  bool tryDlmsSnrm(uint32_t baud, uint8_t *respBuf, size_t maxLen, size_t &outLen);
};

#endif // MIDDE_DDS26D_READER_H
