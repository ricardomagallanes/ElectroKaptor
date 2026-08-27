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

static HardwareSerial SerialRAKLocal(PB7, PB6); // PB7 = RX, PB6 = TX

static String sendAtCmdHardware(const char* cmd, uint32_t timeoutMs = 2000) {
  while (SerialRAKLocal.available()) SerialRAKLocal.read(); // Clear RX buffer
  SerialRAKLocal.print(cmd);
  SerialRAKLocal.print("\r\n");

  String resp = "";
  unsigned long start = millis();
  while (millis() - start < timeoutMs) {
    while (SerialRAKLocal.available()) {
      char c = (char)SerialRAKLocal.read();
      resp += c;
    }
    if (resp.indexOf("OK") >= 0 || resp.indexOf("AT_ERROR") >= 0 || resp.indexOf("+EVT:") >= 0) {
      break;
    }
  }
  return resp;
}

LoRaWANHandler::LoRaWANHandler() : _joined(false) {}

bool LoRaWANHandler::begin() {
  // Habilitar remapeo hardware AFIO de USART1 a PB6(TX) y PB7(RX)
  __HAL_RCC_AFIO_CLK_ENABLE();
  __HAL_AFIO_REMAP_USART1_ENABLE();

  // Reset del RAK3172 en LORA_RST_PIN (PB8 / Pin 45)
  pinMode(LORA_RST_PIN, OUTPUT);
  digitalWrite(LORA_RST_PIN, LOW);  delay(300);
  digitalWrite(LORA_RST_PIN, HIGH); delay(1500);

  SerialRAKLocal.begin(LORA_BAUD);
  delay(200);

  for (int retry = 0; retry < 5; retry++) {
    if (sendAtCmdHardware("AT", 800).indexOf("OK") >= 0) {
      return true;
    }
    delay(300);
  }
  return false;
}

void LoRaWANHandler::process(uint32_t ms) {
  delay(ms);
}

bool LoRaWANHandler::isJoined() {
  String resp = sendAtCmdHardware("AT+NJS=?", 800);
  if (resp.indexOf("1") >= 0) {
    _joined = true;
  }
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

  sendAtCmdHardware("AT+NWM=1", 1500); delay(300);
  sendAtCmdHardware("AT+NJM=1", 1000); delay(300);
  sendAtCmdHardware("AT+BAND=6", 1500); delay(1000); // AU915
  sendAtCmdHardware("AT+MASK=0002", 1000);           // FSB2 (Canales 8-15 usador por TTN)

  snprintf(cmdBuf, sizeof(cmdBuf), "AT+DEVEUI=%s", devEuiStr);
  sendAtCmdHardware(cmdBuf, 1000);

  snprintf(cmdBuf, sizeof(cmdBuf), "AT+APPEUI=%s", appEuiStr);
  sendAtCmdHardware(cmdBuf, 1000);

  snprintf(cmdBuf, sizeof(cmdBuf), "AT+APPKEY=%s", appKeyStr);
  sendAtCmdHardware(cmdBuf, 1000);

  // Detener Join previo y lanzar nuevo Join OTAA
  sendAtCmdHardware("AT+JOIN=0", 500); delay(300);
  sendAtCmdHardware("AT+JOIN=1:0:10:8", 2000);

  unsigned long start = millis();
  while (millis() - start < timeoutMs) {
    while (SerialRAKLocal.available()) {
      String line = SerialRAKLocal.readStringUntil('\n');
      if (line.indexOf("+EVT:JOINED") >= 0 || line.indexOf("JOINED") >= 0) {
        _joined = true;
        return true;
      }
    }
    if (isJoined()) {
      _joined = true;
      return true;
    }
    delay(500);
  }

  _joined = false;
  return false;
}

extern HardwareSerial SerialDebug2;

bool LoRaWANHandler::sendPayload(const uint8_t *payload, uint8_t length, uint8_t port) {
  if (length == 0) return false;

  // 1. Verificar si el módem está unido a la red LoRaWAN (OTAA)
  if (!isJoined()) {
    SerialDebug2.println("[LORAWAN] Módem no unido a la red. Ejecutando OTAA Join...");
    if (!joinOTAA(30000)) {
      SerialDebug2.println("[LORAWAN] Falló el OTAA Join. Se cancela el envío.");
      return false;
    }
  }

  // 2. Convertir payload binario a representación Hex ASCII
  char hexBuf[128] = {0};
  const char hexChars[] = "0123456789ABCDEF";
  for (uint8_t i = 0; i < length; i++) {
    hexBuf[i * 2]     = hexChars[(payload[i] >> 4) & 0x0F];
    hexBuf[i * 2 + 1] = hexChars[payload[i] & 0x0F];
  }

  char cmdBuf[160];
  snprintf(cmdBuf, sizeof(cmdBuf), "AT+SEND=%d:%s", port, hexBuf);
  SerialDebug2.printf("[LORAWAN AT] >> %s\n", cmdBuf);

  // 3. Enviar comando AT con hasta 3 reintentos en caso de estar ocupado
  for (int retry = 0; retry < 3; retry++) {
    String resp = sendAtCmdHardware(cmdBuf, 6000);
    SerialDebug2.printf("[LORAWAN RAK] << %s\n", resp.c_str());

    if (resp.indexOf("OK") >= 0 || resp.indexOf("+EVT:TX_DONE") >= 0) {
      return true;
    } else if (resp.indexOf("NO_NETWORK") >= 0) {
      SerialDebug2.println("[LORAWAN] RAK indica sin red. Reintentando Join...");
      joinOTAA(25000);
    }
    delay(2000);
  }

  return false;
}

#endif
