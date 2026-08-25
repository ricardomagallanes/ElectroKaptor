#include "LoRaWAN_Handler.h"

#if SELECTED_BOARD_MODEL == BOARD_HELTEC_ESP32_S3

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
  
  // SendFrame() devuelve false (0) si el paquete fue enviado al radio correctamente, o true (1) si ocurrió error.
  bool err = SendFrame();
  if (!err) {
    Serial.println("[LoRaWAN] Paquete enviado al transceiver LoRa. Procesando ventanas de recepción RX1/RX2...");
    // Mantener activo el procesador de IRQ del radio durante 6 segundos para ventanas de recepción (RX1/RX2)
    process(6000);
    return true;
  } else {
    Serial.println("[LoRaWAN] Error al enviar frame por el stack LoRaMac.");
    return false;
  }
}

#elif SELECTED_BOARD_MODEL == BOARD_STM32F103C8T6

#if defined(ARDUINO_ARCH_STM32)
HardwareSerial rakSerial(PA3, PA2);
#else
HardwareSerial rakSerial(2);
#endif

LoRaWANHandler::LoRaWANHandler() : _joined(false) {}

bool LoRaWANHandler::begin() {
  Serial.println("[LoRaWAN] Inicializando módem RAK3172 sobre USART2...");
  rakSerial.begin(9600);
  delay(300);
  while (rakSerial.available()) rakSerial.read();

  rakSerial.print("AT\r\n");
  delay(300);

  String resp = "";
  uint32_t start = millis();
  while (millis() - start < 1000) {
    if (rakSerial.available()) {
      resp += (char)rakSerial.read();
    }
  }

  if (resp.indexOf("OK") != -1) {
    Serial.println("[LoRaWAN] Módem RAK3172 responde AT OK.");
    return true;
  }
  Serial.println("[LoRaWAN] RAK3172 inicializado (esperando transmisión).");
  return true;
}

void LoRaWANHandler::process(uint32_t ms) {
  uint32_t start = millis();
  while (millis() - start < ms) {
    while (rakSerial.available()) {
      Serial.write(rakSerial.read());
    }
    delay(10);
  }
}

bool LoRaWANHandler::isJoined() {
  return _joined;
}

bool LoRaWANHandler::joinOTAA(uint32_t timeoutMs) {
  Serial.println("[LoRaWAN] Enviando comando Join OTAA a RAK3172...");
  rakSerial.print("AT+NWM=1\r\n");
  process(200);
  rakSerial.print("AT+BAND=6\r\n");
  process(200);
  rakSerial.print("AT+JOIN=1:0:10:8\r\n");

  uint32_t start = millis();
  while (millis() - start < timeoutMs) {
    process(100);
  }
  _joined = true;
  return true;
}

bool LoRaWANHandler::sendPayload(const uint8_t *payload, uint8_t length, uint8_t port) {
  if (length == 0) return false;

  char hexBuf[128] = {0};
  const char hexChars[] = "0123456789ABCDEF";
  for (uint8_t i = 0; i < length; i++) {
    hexBuf[i * 2]     = hexChars[(payload[i] >> 4) & 0x0F];
    hexBuf[i * 2 + 1] = hexChars[payload[i] & 0x0F];
  }

  Serial.printf("[LoRaWAN] RAK3172 AT+SEND=%d:%s\n", port, hexBuf);
  rakSerial.printf("AT+SEND=%d:%s\r\n", port, hexBuf);
  process(3000);
  return true;
}

#endif
