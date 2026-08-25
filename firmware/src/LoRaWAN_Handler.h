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
  bool joinOTAA(uint32_t timeoutMs = 15000);
  bool sendPayload(const uint8_t *payload, uint8_t length, uint8_t port = 10);
  bool isJoined();
  void process(uint32_t ms = 10);

private:
  bool _joined;
};

#endif // LORAWAN_HANDLER_H
