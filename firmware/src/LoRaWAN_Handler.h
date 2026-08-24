#ifndef LORAWAN_HANDLER_H
#define LORAWAN_HANDLER_H

#include <Arduino.h>
#include "LoRaWan_APP.h"
#include "MeterConfig.h"
#include "BoardConfig.h"
#include "Credentials.h"

class LoRaWANHandler {
public:
  LoRaWANHandler();
  bool begin();
  bool joinOTAA();
  bool sendPayload(const uint8_t *payload, uint8_t length, uint8_t port = 1);
  bool isJoined() const { return _joined; }
  void setJoined(bool joined) { _joined = joined; }

private:
  bool _joined;
};

#endif // LORAWAN_HANDLER_H
