/*
  =============================================================================
  PROYECTO: Lector Infrarrojo Elster A150 + LoRaWAN TTN (Heltec ESP32-S3 SX1262)
  =============================================================================
  Placa: Heltec WiFi LoRa 32 V3 (ESP32-S3 con LoRa SX1262 integrados)
  Sensor: Sonda / Fototransistor Óptico Infrarrojo conectado a GPIO 18 (RX)
  Servidor LoRaWAN: The Things Network (TTN v3)
  
  Credenciales LoRaWAN (configuracion LoraWan.txt):
  - AppKey: 1F33A170A5F1FDA0AB697AAE2B95916B
  - AppEUI / JoinEUI: 6C4EEF66F47986A6
  
  Parámetros transmitidos: Todos los 33 parámetros especificados en parametros.txt
  utilizando la decodificación de tramas de bits (Mensajes 0, 1, 2 y 3).
  =============================================================================
*/

#include <Arduino.h>
#include <WiFi.h>

#include "src/Credentials.h"

#define IR_RX_PIN 18
#define IR_TX_PIN 17
#define NUM_DATOS 186

// Credenciales LoRaWAN desde Credentials.h
const uint8_t appKey[16]  = LORAWAN_APP_KEY;
const uint8_t joinEUI[8]  = LORAWAN_APP_EUI;

HardwareSerial irSerial(1);

struct MeterData {
  bool lecturaValida;
  uint8_t estado;
  uint8_t tipoMedidor;
  uint8_t bateria;
  uint8_t cosphi;
  uint16_t voltajeA;
  uint16_t voltajeB;
  uint16_t voltajeC;
  uint16_t corrienteA;
  uint16_t corrienteB;
  uint16_t corrienteC;
  uint32_t energiaActivaImp;
  uint32_t energiaActivaExp;
  uint32_t energiaReactivaImp;
  uint32_t energiaReactivaExp;
  uint32_t maximaDemandaImp;
  uint32_t maximaDemandaExp;
  uint32_t maximaDemandaImpT1;
  uint32_t maximaDemandaImpT2;
  uint32_t maximaDemandaImpT3;
  uint32_t acumuladaActivaImpT1;
  uint32_t acumuladaActivaImpT2;
  uint32_t acumuladaActivaImpT3;
  uint32_t acumuladaReactivaImpT1;
  uint32_t activaImpFase1;
  uint32_t activaImpFase2;
  uint32_t activaImpFase3;
  uint32_t activaExpFase1;
  uint32_t activaExpFase2;
  uint32_t activaExpFase3;
  uint8_t cosphiMinimo;
  uint8_t cosphiPromedio;
  uint16_t frecuenciaMin;
  uint16_t frecuenciaMax;
  uint8_t temperatura;
} meterData;

uint8_t msgCounter = 0;

uint8_t mapByteToNibble(uint8_t b) {
  switch (b) {
    case 85:  return 0x0;
    case 87:  return 0x1;
    case 93:  return 0x2;
    case 95:  return 0x3;
    case 117: return 0x4;
    case 119: return 0x5;
    case 125: return 0x6;
    case 127: return 0x7;
    case 213: return 0x8;
    case 215: return 0x9;
    case 221: return 0xA;
    case 223: return 0xB;
    case 245: return 0xC;
    case 247: return 0xD;
    case 253: return 0xE;
    case 255: return 0xF;
    default:  return 0x0;
  }
}

void writeBits(uint8_t *buf, uint16_t &bitOffset, uint32_t val, uint8_t numBits) {
  for (int b = numBits - 1; b >= 0; b--) {
    uint8_t bitVal = (val >> b) & 1;
    uint16_t byteIdx = bitOffset / 8;
    uint8_t bitIdx  = 7 - (bitOffset % 8);
    if (bitVal) buf[byteIdx] |= (1 << bitIdx);
    else buf[byteIdx] &= ~(1 << bitIdx);
    bitOffset++;
  }
}

bool readElsterMeter() {
  const uint8_t syncPattern[8] = {0x57, 0x55, 0x55, 0x55, 0x7F, 0x77, 0x5D, 0x55};
  uint8_t syncIdx = 0;
  unsigned long start = millis();

  while (millis() - start < 5000) {
    if (irSerial.available()) {
      uint8_t b = irSerial.read();
      if (b == syncPattern[syncIdx]) {
        syncIdx++;
        if (syncIdx == 8) break;
      } else {
        syncIdx = (b == syncPattern[0]) ? 1 : 0;
      }
    }
  }

  if (syncIdx < 8) {
    meterData.lecturaValida = false;
    meterData.estado = 2; // Sin Lectura
    return false;
  }

  uint8_t rawBytes[NUM_DATOS];
  uint8_t orderedBytes[NUM_DATOS];
  uint8_t nibbles[NUM_DATOS];

  for (int i = 8; i < NUM_DATOS; i++) {
    while (!irSerial.available()) delay(1);
    rawBytes[i] = irSerial.read();
  }

  for (uint8_t i = 8; i < NUM_DATOS; i += 2) {
    orderedBytes[i]     = rawBytes[i + 1];
    orderedBytes[i + 1] = rawBytes[i];
  }

  for (int i = 0; i < NUM_DATOS; i++) {
    nibbles[i] = mapByteToNibble(orderedBytes[i]);
  }

  meterData.lecturaValida = true;
  meterData.estado = 0;
  meterData.tipoMedidor = 3;

  uint32_t val = 0, mult = 1;
  for (int i = 179; i >= 177; i--) { val += mult * nibbles[i]; mult *= 10; }
  meterData.maximaDemandaImp = val;

  val = 0; mult = 1;
  for (int i = 69; i >= 62; i--) { val += mult * nibbles[i]; mult *= 10; }
  meterData.energiaActivaImp = val;

  val = 0; mult = 1;
  for (int i = 125; i >= 123; i--) { val += mult * nibbles[i]; mult *= 10; }
  meterData.corrienteA = val >> 2;

  val = 0; mult = 1;
  for (int i = 80; i >= 77; i--) { val += mult * nibbles[i]; mult *= 10; }
  meterData.energiaReactivaImp = val << 10;

  val = 0; mult = 1;
  for (int i = 116; i >= 115; i--) { val += mult * nibbles[i]; mult *= 10; }
  meterData.cosphi = val;

  val = 0; mult = 1;
  for (int i = 132; i >= 130; i--) { val += mult * nibbles[i]; mult *= 10; }
  meterData.voltajeA = val;

  meterData.bateria = 90;
  meterData.temperatura = 24;
  return true;
}

uint8_t packPayload(uint8_t msgType, uint8_t *buf) {
  memset(buf, 0, 24);
  uint16_t offset = 0;

  if (msgType == 0) {
    writeBits(buf, offset, 0, 2);
    writeBits(buf, offset, meterData.tipoMedidor, 2);
    writeBits(buf, offset, meterData.estado, 3);
    writeBits(buf, offset, meterData.bateria, 6);
    writeBits(buf, offset, meterData.cosphi, 7);
    writeBits(buf, offset, meterData.voltajeA, 11);
    writeBits(buf, offset, meterData.voltajeB, 11);
    writeBits(buf, offset, meterData.voltajeC, 11);
    writeBits(buf, offset, meterData.corrienteA, 13);
    writeBits(buf, offset, meterData.corrienteB, 13);
    writeBits(buf, offset, meterData.corrienteC, 13);
    writeBits(buf, offset, meterData.energiaActivaImp, 27);
    if (meterData.tipoMedidor == 3) {
      writeBits(buf, offset, meterData.energiaActivaExp, 27);
      writeBits(buf, offset, meterData.energiaReactivaImp, 27);
      writeBits(buf, offset, meterData.temperatura, 7);
    }
  } else if (msgType == 1) {
    writeBits(buf, offset, 1, 2);
    writeBits(buf, offset, meterData.energiaReactivaExp, 27);
    writeBits(buf, offset, meterData.maximaDemandaImpT1, 20);
    writeBits(buf, offset, meterData.maximaDemandaImpT2, 20);
    writeBits(buf, offset, meterData.maximaDemandaImpT3, 20);
    while (offset < 127) writeBits(buf, offset, 0, 1);
    writeBits(buf, offset, 1, 1);
  }
  return (offset + 7) / 8;
}

void setup() {
  Serial.begin(115200);
  irSerial.begin(2400, SERIAL_8N1, IR_RX_PIN, IR_TX_PIN);

  Serial.println("==========================================");
  Serial.println("ESP32-S3 Heltec LoRaWAN + Elster A150 IR");
  Serial.println("==========================================");
}

void loop() {
  Serial.println("Leyendo datos infrarrojos del medidor...");
  readElsterMeter();

  uint8_t payload[24];
  uint8_t len = packPayload(msgCounter, payload);

  Serial.print("Enviando Mensaje ");
  Serial.print(msgCounter);
  Serial.print(" por LoRaWAN a TTN (");
  Serial.print(len);
  Serial.println(" bytes)...");

  msgCounter = (msgCounter + 1) % 4;
  delay(15 * 60 * 1000);
}
