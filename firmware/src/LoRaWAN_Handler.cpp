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

char g_rakLog[512] = {0};
static uint16_t g_rakLogIdx = 0;

void appendRakLog(const char* txt) {
  uint16_t len = strlen(txt);
  if (g_rakLogIdx + len >= sizeof(g_rakLog) - 1) {
    g_rakLogIdx = 0;
    memset(g_rakLog, 0, sizeof(g_rakLog));
  }
  memcpy(&g_rakLog[g_rakLogIdx], txt, len);
  g_rakLogIdx += len;
  g_rakLog[g_rakLogIdx] = '\0';
}

static String sendAtCmdHardware(const char* cmd, uint32_t timeoutMs = 2000) {
  while (SerialRAKLocal.available()) SerialRAKLocal.read(); // Clear RX buffer
  
  appendRakLog("\n>> ");
  appendRakLog(cmd);
  appendRakLog("\n");

  char fullCmd[200];
  snprintf(fullCmd, sizeof(fullCmd), "%s\r\n", cmd);
  SerialRAKLocal.write((const uint8_t*)fullCmd, strlen(fullCmd));

  String resp = "";
  unsigned long start = millis();
  while (millis() - start < timeoutMs) {
    while (SerialRAKLocal.available()) {
      char c = (char)SerialRAKLocal.read();
      resp += c;
    }
    if (resp.indexOf("OK") >= 0 || resp.indexOf("AT_ERROR") >= 0 || resp.indexOf("AT_PARAM_ERROR") >= 0 || resp.indexOf("+EVT:") >= 0) {
      break;
    }
  }

  appendRakLog("<< ");
  appendRakLog(resp.c_str());

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
  delay(300);

  bool rakReady = false;
  for (int retry = 0; retry < 5; retry++) {
    if (sendAtCmdHardware("AT", 800).indexOf("OK") >= 0) {
      rakReady = true;
      break;
    }
    delay(400);
  }

  if (!rakReady) return false;

  // Configurar parámetros de Red y Credenciales OTAA UNA SOLA VEZ
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

  sendAtCmdHardware("AT+NWM=1", 1500); delay(400);
  sendAtCmdHardware("AT+NJM=1", 1500); delay(1000); // Aguardar banner RUI3
  sendAtCmdHardware("AT+BAND=6", 1500); delay(500);  // Banda AU915
  sendAtCmdHardware("AT+MASK=0002", 1000);            // Sub-banda 2 FSB2 (Canales 8-15)
  sendAtCmdHardware("AT+CFM=0", 1000);                // Modo Unconfirmed (Telemetría periódica sin ACK)
  sendAtCmdHardware("AT+ADR=1", 1000);                // Adaptive Data Rate

  snprintf(cmdBuf, sizeof(cmdBuf), "AT+DEVEUI=%s", devEuiStr);
  sendAtCmdHardware(cmdBuf, 1000);

  snprintf(cmdBuf, sizeof(cmdBuf), "AT+APPEUI=%s", appEuiStr);
  sendAtCmdHardware(cmdBuf, 1000);

  snprintf(cmdBuf, sizeof(cmdBuf), "AT+APPKEY=%s", appKeyStr);
  sendAtCmdHardware(cmdBuf, 1000);

  // Lanzar AutoJoin permanente en RAK3172 (1: Join, 1: AutoJoin ON, 10s intervalo, 8 reintentos)
  sendAtCmdHardware("AT+JOIN=1:1:10:8", 2000);

  return true;
}

bool LoRaWANHandler::isJoined() {
  // 1. Escuchar eventos asíncronos pendientes en el buffer serie del RAK3172
  while (SerialRAKLocal.available()) {
    String line = SerialRAKLocal.readStringUntil('\n');
    if (line.indexOf("+EVT:JOINED") >= 0 || line.indexOf("JOINED") >= 0) {
      _joined = true;
      _failCount = 0;
      return true;
    } else if (line.indexOf("+EVT:JOIN_FAILED") >= 0) {
      _joined = false;
    }
  }

  // 2. Consultar estado del stack LoRaWAN de forma estricta
  String resp = sendAtCmdHardware("AT+NJS=?", 800);
  if (resp.indexOf("AT+NJS=1") >= 0 || resp.indexOf("NJS=1") >= 0 || resp.indexOf("=1") >= 0) {
    _joined = true;
  } else {
    _joined = false;
  }
  return _joined;
}

bool LoRaWANHandler::joinOTAA(uint32_t timeoutMs) {
  if (isJoined()) return true;

  sendAtCmdHardware("AT+JOIN=1:1:10:8", 2000);
  unsigned long start = millis();
  while (millis() - start < timeoutMs) {
    if (isJoined()) return true;
    delay(1000);
  }
  return isJoined();
}

#include "DebugSerial.h"

bool LoRaWANHandler::sendPayload(const uint8_t *payload, uint8_t length, uint8_t port) {
  if (length == 0) return false;

  // 1. Verificar que el módem esté efectivamente unido a la red
  if (!_joined && !isJoined()) {
    debugPrintln("[LORAWAN] Módem no unido a TTN. Omitiendo envío hasta completar Join.");
    return false;
  }

  // 2. Convertir payload binario a representación Hex ASCII
  char hexBuf[128] = {0};
  const char hexChars[] = "0123456789ABCDEF";
  for (uint8_t i = 0; i < length; i++) {
    hexBuf[i * 2]     = hexChars[(payload[i] >> 4) & 0x0F];
    hexBuf[i * 2 + 1] = hexChars[payload[i] & 0x0F];
  }
  hexBuf[length * 2] = '\0';

  char cmdBuf[160];
  snprintf(cmdBuf, sizeof(cmdBuf), "AT+SEND=%u:%s", (unsigned int)port, hexBuf);
  debugPrintf("[LORAWAN AT] >> %s\n", cmdBuf);

  // 3. Transmisión del paquete
  for (int retry = 0; retry < 2; retry++) {
    String resp = sendAtCmdHardware(cmdBuf, 4000);
    debugPrintf("[LORAWAN RAK] << %s\n", resp.c_str());

    if (resp.indexOf("OK") >= 0 || resp.indexOf("+EVT:TX_DONE") >= 0) {
      _failCount = 0;
      _joined = true;
      delay(3000); // Pausa para finalizar transmisión RF y ventanas de recepción RX1/RX2
      return true;
    }
    delay(1000);
  }

  _failCount++;
  debugPrintf("[LORAWAN] Fallo de transmisión acumulado: %d\n", _failCount);

  // Si fallan 2 intentos consecutivos (ej: gateway apagado), solicitar re-unión automática
  if (_failCount >= 2) {
    _joined = false;
    debugPrintln("[LORAWAN] Reiniciando solicitud de Join OTAA en segundo plano...");
    sendAtCmdHardware("AT+JOIN=1:1:10:8", 2000);
  }

  return false;
}

#endif

