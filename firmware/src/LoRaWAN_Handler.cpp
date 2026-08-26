#include "LoRaWAN_Handler.h"

#if defined(ARDUINO_ARCH_ESP32)

#include <WiFi.h>

/* Licencia activa Heltec para ESP32-S3 V3 (definida en Credentials.h local) */
uint32_t license[4] = HELTEC_LICENSE;

/* Credenciales OTAA para TTN LNS (AU915) */
uint8_t devEui[] = LORAWAN_DEV_EUI;
uint8_t appEui[] = LORAWAN_APP_EUI;
uint8_t appKey[] = LORAWAN_APP_KEY;

/* Parametros ABP (no utilizados en OTAA pero requeridos por la estructura Heltec) */
uint8_t nwkSKey[16] = LORAWAN_NWK_S_KEY;
uint8_t appSKey[16] = LORAWAN_APP_S_KEY;
uint32_t devAddr = LORAWAN_DEV_ADDR;

/* Máscara de canales LoRaWAN para AU915 FSB2 (Canales 8-15) segun configuracion TTN */
uint16_t userChannelsMask[6] = { 0xFF00, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000 };

/* Región activa LoRaWAN */
LoRaMacRegion_t loraWanRegion = ACTIVE_REGION;

/* Clase LoRaWAN (Clase A) */
DeviceClass_t loraWanClass = CLASS_A;

/* Ciclo de transmisión (ms) */
uint32_t appTxDutyCycle = 15000;

/* Activación OTAA */
bool overTheAirActivation = true;

/* ADR activado */
bool loraWanAdr = true;

/* Mensajes no confirmados */
bool isTxConfirmed = false;

/* Puerto de aplicación */
uint8_t appPort = 10;

/* Número de reintentos */
uint8_t confirmedNbTrials = 4;

LoRaWANHandler::LoRaWANHandler() : _joined(false) {}

bool LoRaWANHandler::begin() {
  Serial.println("[LoRaWAN] Inicializando pila oficial Heltec LoRaWan_APP con Licencia...");
  
  Serial.print("[LoRaWAN] DevEUI: ");
  for (int i = 0; i < 8; i++) {
    if (devEui[i] < 0x10) Serial.print("0");
    Serial.print(devEui[i], HEX);
  }
  Serial.println();

  Serial.print("[LoRaWAN] AppEUI / JoinEUI: ");
  for (int i = 0; i < 8; i++) {
    if (appEui[i] < 0x10) Serial.print("0");
    Serial.print(appEui[i], HEX);
  }
  Serial.println();

  Serial.print("[LoRaWAN] AppKey: ");
  for (int i = 0; i < 16; i++) {
    if (appKey[i] < 0x10) Serial.print("0");
    Serial.print(appKey[i], HEX);
  }
  Serial.println();

  Mcu.begin(HELTEC_BOARD, SLOW_CLK_TPYE);
  deviceState = DEVICE_STATE_INIT;
  return true;
}

void LoRaWANHandler::process(uint32_t ms) {
  uint32_t start = millis();
  while (millis() - start < ms) {
    Radio.IrqProcess();
    Mcu.timerhandler();
    delay(10);
  }
}

bool LoRaWANHandler::isJoined() {
  MibRequestConfirm_t mibReq;
  mibReq.Type = MIB_NETWORK_JOINED;
  if (LoRaMacMibGetRequestConfirm(&mibReq) == LORAMAC_STATUS_OK) {
    _joined = mibReq.Param.IsNetworkJoined;
  } else {
    _joined = false;
  }
  return _joined;
}

bool LoRaWANHandler::joinOTAA(uint32_t timeoutMs) {
  Serial.println("[LoRaWAN] Inicializando LoRaWAN e iniciando transmisión de Join Request...");
  LoRaWAN.init(loraWanClass, loraWanRegion);
  LoRaWAN.join();

  Serial.printf("[LoRaWAN] Esperando respuesta Join-Accept de TTN (timeout: %u ms)...\n", timeoutMs);
  uint32_t start = millis();
  while (millis() - start < timeoutMs) {
    process(100);
    if (isJoined()) {
      Serial.println("[LoRaWAN] ¡¡¡CONECTADO EXITOSAMENTE A LA RED TTN (OTAA Join-Accept Recibido)!!!");
      return true;
    }
  }

  Serial.println("[LoRaWAN] ERROR: No se recibió Join-Accept de TTN a tiempo.");
  return false;
}

bool LoRaWANHandler::sendPayload(const uint8_t *payload, uint8_t length, uint8_t port) {
  if (!isJoined()) {
    Serial.println("[LoRaWAN] Alerta: Intentando enviar payload sin estar unido a la red. Reintentando Join...");
    if (!joinOTAA(15000)) {
      return false;
    }
  }

  if (length > LORAWAN_APP_DATA_MAX_SIZE) {
    Serial.println("[LoRaWAN] Error: Payload excede tamaño máximo.");
    return false;
  }

  appDataSize = length;
  appPort = port;
  memcpy(appData, payload, length);

  Serial.printf("[LoRaWAN] Transmitiendo %d bytes por Heltec LoRaWAN stack (Puerto %d)...\n", length, port);
  
  bool err = SendFrame();
  if (!err) {
    Serial.println("[LoRaWAN] Paquete enviado al transceiver LoRa. Procesando ventanas de recepción RX1/RX2...");
    process(6000);
    return true;
  } else {
    Serial.println("[LoRaWAN] Error al enviar frame por el stack LoRaMac.");
    return false;
  }
}

#else // ARDUINO_ARCH_STM32 (Placa Principal STM32F103C8T6 con módem RAK3172)

static void delayUsLocal(uint32_t us) {
  uint32_t cycles = (SystemCoreClock / 1000000) * us;
  uint32_t start = DWT->CYCCNT;
  while ((DWT->CYCCNT - start) < cycles);
}

static void txByteLocal(uint8_t pin, uint8_t val, uint32_t bitUs) {
  digitalWrite(pin, LOW);
  delayUsLocal(bitUs);

  for (int i = 0; i < 8; i++) {
    digitalWrite(pin, (val >> i) & 1 ? HIGH : LOW);
    delayUsLocal(bitUs);
  }

  digitalWrite(pin, HIGH);
  delayUsLocal(bitUs * 2);
}

static void sendTxStrLocal(uint8_t pin, const char* str, uint32_t baud = 115200) {
  uint32_t bitUs = 1000000 / baud;
  pinMode(pin, OUTPUT);
  digitalWrite(pin, HIGH);
  delayUsLocal(bitUs * 2);

  while (*str) {
    txByteLocal(pin, (uint8_t)*str++, bitUs);
  }
}

static bool readRxStrLocal(uint8_t pin, uint32_t baud, char* outBuf, uint16_t maxLen, uint32_t timeoutMs = 2000) {
  uint32_t bitUs = 1000000 / baud;
  pinMode(pin, INPUT_PULLUP);

  unsigned long start = millis();
  uint16_t idx = 0;
  memset(outBuf, 0, maxLen);

  while (millis() - start < timeoutMs) {
    if (digitalRead(pin) == LOW) {
      delayUsLocal(bitUs / 2);
      if (digitalRead(pin) != LOW) continue;

      uint8_t val = 0;
      for (int i = 0; i < 8; i++) {
        delayUsLocal(bitUs);
        if (digitalRead(pin) == HIGH) val |= (1 << i);
      }
      delayUsLocal(bitUs * 2);

      if (val >= 32 && val <= 126 && idx < maxLen - 1) {
        outBuf[idx++] = (char)val;
      }
      start = millis();
    }
  }

  return (idx > 0);
}

static bool sendAtCmdLocal(const char* cmd, const char* expectedStr, uint32_t timeoutMs = 2000) {
  sendTxStrLocal(LORA_TX_PIN, cmd, 115200);
  sendTxStrLocal(LORA_TX_PIN, "\r\n", 115200);

  char rxBuf[128];
  if (readRxStrLocal(LORA_RX_PIN, 115200, rxBuf, sizeof(rxBuf), timeoutMs)) {
    if (strstr(rxBuf, expectedStr) != NULL) {
      return true;
    }
  }
  return false;
}

LoRaWANHandler::LoRaWANHandler() : _joined(false) {}

bool LoRaWANHandler::begin() {
  CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
  DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;

  // Pulso Reset del módem RAK3172 en LORA_RST_PIN (PA1)
  pinMode(LORA_RST_PIN, OUTPUT);
  digitalWrite(LORA_RST_PIN, LOW);  delay(30);
  digitalWrite(LORA_RST_PIN, HIGH); delay(200);

  // Sincronización de baudios
  sendTxStrLocal(LORA_TX_PIN, "\r\n", 115200); delay(100);

  if (sendAtCmdLocal("AT", "OK", 1000)) {
    return true;
  }
  return false;
}

void LoRaWANHandler::process(uint32_t ms) {
  delay(ms);
}

bool LoRaWANHandler::isJoined() {
  return _joined;
}

bool LoRaWANHandler::joinOTAA(uint32_t timeoutMs) {
  uint8_t devEuiBytes[] = LORAWAN_DEV_EUI;
  uint8_t appEuiBytes[] = LORAWAN_APP_EUI;
  uint8_t appKeyBytes[] = LORAWAN_APP_KEY;

  char devEuiStr[17], appEuiStr[17], appKeyStr[33], cmdBuf[128];
  const char hexChars[] = "0123456789ABCDEF";

  for (uint8_t i = 0; i < 8; i++) {
    devEuiStr[i * 2]     = hexChars[(devEuiBytes[i] >> 4) & 0x0F];
    devEuiStr[i * 2 + 1] = hexChars[devEuiBytes[i] & 0x0F];
    appEuiStr[i * 2]     = hexChars[(appEuiBytes[i] >> 4) & 0x0F];
    appEuiStr[i * 2 + 1] = hexChars[appEuiBytes[i] & 0x0F];
  }
  devEuiStr[16] = '\0';
  appEuiStr[16] = '\0';

  for (uint8_t i = 0; i < 16; i++) {
    appKeyStr[i * 2]     = hexChars[(appKeyBytes[i] >> 4) & 0x0F];
    appKeyStr[i * 2 + 1] = hexChars[appKeyBytes[i] & 0x0F];
  }
  appKeyStr[32] = '\0';

  sendAtCmdLocal("AT+NWM=1", "OK", 1000);
  sendAtCmdLocal("AT+BAND=6", "OK", 1000);   // AU915
  sendAtCmdLocal("AT+MASK=0002", "OK", 1000); // FSB2 (Canales 8-15 usados por TTN)
  sendAtCmdLocal("AT+CHE=2", "OK", 1000);     // FSB2 (Configuración alternativa RAK)

  snprintf(cmdBuf, sizeof(cmdBuf), "AT+DEVEUI=%s", devEuiStr);
  sendAtCmdLocal(cmdBuf, "OK", 1000);

  snprintf(cmdBuf, sizeof(cmdBuf), "AT+APPEUI=%s", appEuiStr);
  sendAtCmdLocal(cmdBuf, "OK", 1000);

  snprintf(cmdBuf, sizeof(cmdBuf), "AT+APPKEY=%s", appKeyStr);
  sendAtCmdLocal(cmdBuf, "OK", 1000);

  // Transmitir Join OTAA
  sendAtCmdLocal("AT+JOIN=1:0:10:8", "OK", 2000);

  char joinRx[128];
  unsigned long start = millis();
  while (millis() - start < timeoutMs) {
    if (readRxStrLocal(LORA_RX_PIN, 115200, joinRx, sizeof(joinRx), 2000)) {
      if (strstr(joinRx, "+EVT:JOINED") != NULL || strstr(joinRx, "JOINED") != NULL || strstr(joinRx, "OK") != NULL) {
        _joined = true;
        return true;
      }
    }
  }

  _joined = false;
  return false;
}

bool LoRaWANHandler::sendPayload(const uint8_t *payload, uint8_t length, uint8_t port) {
  if (length == 0) return false;

  char hexBuf[128] = {0};
  const char hexChars[] = "0123456789ABCDEF";
  for (uint8_t i = 0; i < length; i++) {
    hexBuf[i * 2]     = hexChars[(payload[i] >> 4) & 0x0F];
    hexBuf[i * 2 + 1] = hexChars[payload[i] & 0x0F];
  }

  char cmdBuf[160];
  snprintf(cmdBuf, sizeof(cmdBuf), "AT+SEND=%d:%s", port, hexBuf);
  return sendAtCmdLocal(cmdBuf, "OK", 5000);
}

#endif
