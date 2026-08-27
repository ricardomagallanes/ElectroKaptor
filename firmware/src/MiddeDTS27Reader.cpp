#include "MiddeDTS27Reader.h"
#include "MeterConfig.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BIT_TIME_US 3333 // 300 baudios (1/300 = 3333.33 us por bit)

MiddeDTS27Reader::MiddeDTS27Reader(uint8_t rxPin, uint8_t txPin) 
  : _rxPin(rxPin), _txPin(txPin) {}

void MiddeDTS27Reader::begin(unsigned long baudRate) {
  pinMode(_txPin, OUTPUT);
  digitalWrite(_txPin, HIGH); // Idle HIGH para TX Infrarrojo
  pinMode(_rxPin, INPUT_PULLUP);
}

void MiddeDTS27Reader::sendCharBitbang(char c) {
  uint8_t val = (uint8_t)c & 0x7F;
  uint8_t parity = 0;
  for (int i = 0; i < 7; i++) {
    if (val & (1 << i)) parity ^= 1;
  }

  // Start bit: LOW
  digitalWrite(_txPin, LOW);
  delayMicroseconds(BIT_TIME_US);

  // 7 Data bits
  for (int i = 0; i < 7; i++) {
    bool bitVal = (val >> i) & 1;
    digitalWrite(_txPin, bitVal ? HIGH : LOW);
    delayMicroseconds(BIT_TIME_US);
  }

  // Parity bit (Even Parity 7E1)
  digitalWrite(_txPin, parity ? HIGH : LOW);
  delayMicroseconds(BIT_TIME_US);

  // Stop bit: HIGH
  digitalWrite(_txPin, HIGH);
  delayMicroseconds(BIT_TIME_US * 2);
}

void MiddeDTS27Reader::sendStringBitbang(const char* str) {
  while (*str) {
    sendCharBitbang(*str++);
  }
}

int MiddeDTS27Reader::readCharBitbang(unsigned long timeoutMs) {
  unsigned long start = millis();

  while (millis() - start < timeoutMs) {
    // Detectar flanco de inicio (LOW en lógica normal o HIGH en lógica invertida)
    bool idleState = digitalRead(_rxPin);
    if (idleState == LOW) { // Flanco de bajada
      delayMicroseconds(BIT_TIME_US / 2);
      if (digitalRead(_rxPin) != LOW) continue;

      uint8_t val = 0;
      for (int i = 0; i < 7; i++) {
        delayMicroseconds(BIT_TIME_US);
        if (digitalRead(_rxPin) == HIGH) {
          val |= (1 << i);
        }
      }
      delayMicroseconds(BIT_TIME_US * 3); // Saltear paridad y stop bit
      return val & 0x7F;
    }
  }
  return -1;
}

bool MiddeDTS27Reader::readMeter(MeterData &data, unsigned long timeoutMs) {
  memset(&data, 0, sizeof(MeterData));
  data.lecturaValida = false;
  data.tipoMedidor = 2; // Trifásico (MIDDE DTS27)
  data.estado = 0;      // Normal

  begin(300);
  delay(100);

  // 1. Envío de comando Sign-on (/?!\r\n)
  sendStringBitbang("/?!\r\n");

  // 2. Captura de la respuesta de identificación (/SCZ5... o /XXX5...)
  unsigned long timeout = millis() + 3500;
  char idBuffer[64];
  uint16_t idIdx = 0;
  memset(idBuffer, 0, sizeof(idBuffer));

  while (millis() < timeout) {
    int b = readCharBitbang(150);
    if (b >= 0) {
      char c = (char)(b & 0x7F);
      if (idIdx < sizeof(idBuffer) - 1) idBuffer[idIdx++] = c;
      if (c == '\n') break;
    }
  }

  if (idIdx == 0) {
    data.estado = 2; // Sin Lectura
    return false;
  }

  // 3. Enviar ACK de solicitud de volcado de datos: ACK(0x06) + "000\r\n"
  delay(150);
  sendCharBitbang(0x06);
  sendStringBitbang("000\r\n");

  // 4. Captura y parseo del bloque de registros OBIS
  timeout = millis() + 7000;
  char lineBuf[64];
  uint16_t lineIdx = 0;
  memset(lineBuf, 0, sizeof(lineBuf));

  while (millis() < timeout) {
    int b = readCharBitbang(150);
    if (b >= 0) {
      char c = (char)(b & 0x7F);

      if (c == '\r' || c == '\n') {
        if (lineIdx > 0) {
          lineBuf[lineIdx] = '\0';

          // Parseo de campos OBIS
          float valFloat = 0.0f;
          char* p1 = strchr(lineBuf, '(');
          char* p2 = p1 ? strchr(p1, '*') : NULL;

          if (p1 && p2) {
            *p2 = '\0';
            valFloat = atof(p1 + 1);

            if (strstr(lineBuf, "1.0.32.7.0") != NULL) {
              data.voltajeA = (uint16_t)valFloat;
            } else if (strstr(lineBuf, "1.0.52.7.0") != NULL) {
              data.voltajeB = (uint16_t)valFloat;
            } else if (strstr(lineBuf, "1.0.72.7.0") != NULL) {
              data.voltajeC = (uint16_t)valFloat;
            } else if (strstr(lineBuf, "1.0.1.8.0") != NULL || strstr(lineBuf, "1.0.15.8.0") != NULL) {
              data.energiaActivaImp = (uint32_t)(valFloat * 100.0f);
              data.lecturaValida = true;
            }
          }

          if (lineBuf[0] == '!') break;
          lineIdx = 0;
          memset(lineBuf, 0, sizeof(lineBuf));
        }
      } else if (lineIdx < sizeof(lineBuf) - 1) {
        lineBuf[lineIdx++] = c;
      }
    }
  }

  return data.lecturaValida;
}
