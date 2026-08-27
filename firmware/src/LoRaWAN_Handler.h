#ifndef LORAWAN_HANDLER_H
#define LORAWAN_HANDLER_H

#include <Arduino.h>
#include "BoardConfig.h"
#include "Credentials.h"

#if defined(ARDUINO_ARCH_ESP32)
#include "LoRaWan_APP.h"
#endif

class LoRaWANHandler {
public:
  LoRaWANHandler();
  bool begin();
  bool joinOTAA(uint32_t timeoutMs = 15000);
  bool sendPayload(const uint8_t *payload, uint8_t length, uint8_t port = 10, bool confirmed = false);
  bool isJoined();
  void process(uint32_t ms = 10);

private:
  bool _joined;
  uint8_t _failCount;
};

#endif // LORAWAN_HANDLER_H
