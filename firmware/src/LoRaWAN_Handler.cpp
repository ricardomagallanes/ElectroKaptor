#include "LoRaWAN_Handler.h"
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
uint8_t appPort = 2;

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

bool LoRaWANHandler::joinOTAA() {
  Serial.println("[LoRaWAN] Ejecutando Join OTAA por la libreria Heltec...");
  LoRaWAN.init(loraWanClass, loraWanRegion);
  LoRaWAN.join();
  _joined = true;
  return true;
}

bool LoRaWANHandler::sendPayload(const uint8_t *payload, uint8_t length, uint8_t port) {
  if (length > LORAWAN_APP_DATA_MAX_SIZE) {
    Serial.println("[LoRaWAN] Error: Payload excede tamaño máximo.");
    return false;
  }

  appDataSize = length;
  appPort = port;
  memcpy(appData, payload, length);
  
  Serial.printf("[LoRaWAN] Transmitiendo %d bytes por Heltec LoRaWAN stack...\n", length);
  LoRaWAN.send();
  return true;
}
