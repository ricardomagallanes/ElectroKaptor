#include "MeterAutoDetector.h"
#include "DebugSerial.h"

// 300 baudios: 3333 us por bit
#define BIT_TIME_US_300  3333
// 2400 baudios: 417 us por bit
#define BIT_TIME_US_2400 417

volatile AutoDetectDiag g_autoDetectDiag = {
  0xAD07EC70, 0, 0, 0, 0, {0}, {0}
};

const char* MeterAutoDetector::getModelName(uint8_t model) {
  switch (model) {
    case METER_MODEL_ELSTER_A150:   return "Elster A150 (Monofasico)";
    case METER_MODEL_MIDDE_DTS27:   return "MIDDE DTS27 (Trifasico)";
    case METER_MODEL_MIDDE_DDS26D:  return "MIDDE DDS26D (Monofasico)";
    case METER_MODEL_ELSTER_A1052:  return "Elster A1052 (Trifasico)";
    case METER_MODEL_HEXING_HXE34K: return "Hexing HXE34K (Trifasico)";
    default:                        return "Desconocido";
  }
}

void MeterAutoDetector::sendOpticalWakeUp(uint8_t txPin, uint16_t durationMs) {
  unsigned long start = millis();
  while (millis() - start < durationMs) {
    digitalWrite(txPin, LOW);
    delayMicroseconds(500);
    digitalWrite(txPin, HIGH);
    delayMicroseconds(500);
  }
  digitalWrite(txPin, HIGH);
  delay(200); // Estabilización
}

void MeterAutoDetector::sendCharBitbang7E1(uint8_t txPin, char c, uint32_t bitTimeUs) {
  uint8_t val = (uint8_t)c & 0x7F;
  uint8_t parity = 0;
  for (int i = 0; i < 7; i++) {
    if (val & (1 << i)) parity ^= 1;
  }

  // Start bit (LOW)
  digitalWrite(txPin, LOW);
  delayMicroseconds(bitTimeUs);

  // 7 bits de datos
  for (int i = 0; i < 7; i++) {
    digitalWrite(txPin, (val & (1 << i)) ? HIGH : LOW);
    delayMicroseconds(bitTimeUs);
  }

  // Paridad par
  digitalWrite(txPin, parity ? HIGH : LOW);
  delayMicroseconds(bitTimeUs);

  // Stop bit (HIGH)
  digitalWrite(txPin, HIGH);
  delayMicroseconds(bitTimeUs * 2);
}

void MeterAutoDetector::sendStringBitbang7E1(uint8_t txPin, const char* str, uint32_t bitTimeUs) {
  while (*str) {
    sendCharBitbang7E1(txPin, *str++, bitTimeUs);
  }
}

int MeterAutoDetector::readCharBitbang7E1(uint8_t rxPin, unsigned long timeoutMs, uint32_t bitTimeUs) {
  unsigned long start = millis();
  while (millis() - start < timeoutMs) {
    if (digitalRead(rxPin) == LOW) {
      uint32_t tStart = micros();
      while ((int32_t)(micros() - (tStart + bitTimeUs / 2)) < 0);
      if (digitalRead(rxPin) != LOW) continue; // Falso disparo

      uint8_t val = 0;
      for (int i = 0; i < 7; i++) {
        uint32_t tSample = tStart + (uint32_t)((i + 1) * bitTimeUs) + (bitTimeUs / 2);
        while ((int32_t)(micros() - tSample) < 0);
        if (digitalRead(rxPin) == HIGH) {
          val |= (1 << i);
        }
      }

      // Parity & Stop slots
      uint32_t tParity = tStart + (uint32_t)(8 * bitTimeUs) + (bitTimeUs / 2);
      while ((int32_t)(micros() - tParity) < 0);

      uint32_t tStop = tStart + (uint32_t)(9 * bitTimeUs) + (bitTimeUs / 2);
      while ((int32_t)(micros() - tStop) < 0);

      uint32_t tIdle = micros();
      while (digitalRead(rxPin) == LOW && (micros() - tIdle < bitTimeUs * 2));

      return val & 0x7F;
    }
  }
  return -1;
}

int MeterAutoDetector::readByteBitbang8N1(uint8_t rxPin, unsigned long timeoutMs, uint32_t bitTimeUs) {
  unsigned long start = millis();
  while (millis() - start < timeoutMs) {
    if (digitalRead(rxPin) == LOW) {
      uint32_t tStart = micros();
      while ((int32_t)(micros() - (tStart + bitTimeUs / 2)) < 0);
      if (digitalRead(rxPin) != LOW) continue;

      uint8_t val = 0;
      for (int i = 0; i < 8; i++) {
        uint32_t tSample = tStart + (uint32_t)((i + 1) * bitTimeUs) + (bitTimeUs / 2);
        while ((int32_t)(micros() - tSample) < 0);
        if (digitalRead(rxPin) == HIGH) {
          val |= (1 << i);
        }
      }

      uint32_t tStop = tStart + (uint32_t)(9 * bitTimeUs) + (bitTimeUs / 2);
      while ((int32_t)(micros() - tStop) < 0);

      uint32_t tIdle = micros();
      while (digitalRead(rxPin) == LOW && (micros() - tIdle < bitTimeUs * 2));

      return val;
    }
  }
  return -1;
}

uint8_t MeterAutoDetector::detectMeter(uint8_t rxPin, uint8_t txPin, unsigned long timeoutMs) {
  debugPrintln("\n=======================================================");
  debugPrintln("🔍 [AUTO-DETECT] INICIANDO RECONOCIMIENTO DE MEDIDOR...");
  debugPrintln("=======================================================");

  pinMode(txPin, OUTPUT);
  digitalWrite(txPin, HIGH);
  pinMode(rxPin, INPUT_PULLUP);

  g_autoDetectDiag.magic = 0xAD07EC70;
  g_autoDetectDiag.detectedModel = 0;
  g_autoDetectDiag.attempts++;
  memset((char*)g_autoDetectDiag.modelName, 0, sizeof(g_autoDetectDiag.modelName));
  memset((char*)g_autoDetectDiag.signature, 0, sizeof(g_autoDetectDiag.signature));

  // =========================================================================
  // FASE 1: ESCUCHA PASIVA A 2400 BAUDIOS (ELSTER A1052 / ELSTER A150)
  // =========================================================================
  debugPrintln("[AUTO-DETECT] Fase 1: Escuchando transmisiones espontáneas a 2400 baud...");
  g_autoDetectDiag.detectionStage = 1;

  unsigned long passiveStart = millis();
  bool activityFound = false;
  while (millis() - passiveStart < 2200) {
    if (digitalRead(rxPin) == LOW) {
      activityFound = true;
      break;
    }
  }

  if (activityFound) {
    debugPrintln("[AUTO-DETECT] Actividad detectada a 2400 baud. Analizando ráfaga...");
    String asciiBuf = "";
    int asciiCount = 0;
    int binaryCount = 0;
    unsigned long burstTimeout = millis() + 1500;

    while (millis() < burstTimeout && asciiBuf.length() < 120) {
      int c = readCharBitbang7E1(rxPin, 150, BIT_TIME_US_2400);
      if (c >= 0) {
        if (c >= 32 && c <= 126) {
          asciiBuf += (char)c;
          asciiCount++;
        } else if (c == '\r' || c == '\n') {
          asciiBuf += (char)c;
        } else {
          binaryCount++;
        }
      }
    }

    debugPrintf("[AUTO-DETECT] Muestras: ASCII=%d, Binario=%d | Stream: %s\n", asciiCount, binaryCount, asciiBuf.c_str());

    // Elster A1052 transmite texto ASCII con registros OBIS (puntos, paréntesis, códigos)
    if (asciiBuf.indexOf("32.7") >= 0 || asciiBuf.indexOf("1.8.0") >= 0 || asciiBuf.indexOf("1052") >= 0 ||
        (asciiCount > 15 && asciiBuf.indexOf("(") >= 0)) {
      debugPrintln("✅ [AUTO-DETECT] ¡Identificado Medidor Trifásico ELSTER A1052!");
      g_autoDetectDiag.detectedModel = METER_MODEL_ELSTER_A1052;
      g_autoDetectDiag.detectionStage = 4;
      strncpy((char*)g_autoDetectDiag.modelName, "Elster A1052 (Trifasico)", sizeof(g_autoDetectDiag.modelName) - 1);
      strncpy((char*)g_autoDetectDiag.signature, asciiBuf.substring(0, 60).c_str(), sizeof(g_autoDetectDiag.signature) - 1);
      return METER_MODEL_ELSTER_A1052;
    }

    // Elster A150 transmite tramas continuas de 186 bytes desmoduladas (alta densidad binaria o ráfaga)
    if (binaryCount > 8 || asciiCount > 10) {
      debugPrintln("✅ [AUTO-DETECT] ¡Identificado Medidor Monofásico ELSTER A150!");
      g_autoDetectDiag.detectedModel = METER_MODEL_ELSTER_A150;
      g_autoDetectDiag.detectionStage = 4;
      strncpy((char*)g_autoDetectDiag.modelName, "Elster A150 (Monofasico)", sizeof(g_autoDetectDiag.modelName) - 1);
      return METER_MODEL_ELSTER_A150;
    }
  }

  // =========================================================================
  // FASE 2: SONDEO ACTIVO A 300 BAUDIOS (HEXING HXE34K / MIDDE DTS27)
  // =========================================================================
  debugPrintln("[AUTO-DETECT] Fase 2: Interrogación óptica IEC 62056-21 Modo C a 300 baud...");
  g_autoDetectDiag.detectionStage = 2;

  // 1. Despertar óptico (Wake-up burst de 250 ms)
  sendOpticalWakeUp(txPin, 250);

  // Limpiar residuos en buffer de recepción
  while (readCharBitbang7E1(rxPin, 40, BIT_TIME_US_300) >= 0);

  // 2. Transmitir comando Sign-on (/?!\r\n) a 300 baudios 7E1
  debugPrintln("[AUTO-DETECT] Enviando Sign-on (/?!\\r\\n) a 300 baud...");
  sendStringBitbang7E1(txPin, "/?!\r\n", BIT_TIME_US_300);

  // 3. Capturar respuesta de identificación del medidor (/XXX...)
  String id300 = "";
  unsigned long wait300 = millis() + 4000;
  while (millis() < wait300 && id300.length() < 32) {
    int c = readCharBitbang7E1(rxPin, 500, BIT_TIME_US_300);
    if (c >= 0) {
      char ch = (char)c;
      id300 += ch;
      if (ch == '\n') break;
    } else if (id300.length() > 0) {
      break;
    }
  }

  id300.trim();
  if (id300.length() > 0 && id300.startsWith("/")) {
    debugPrintf("[AUTO-DETECT] Respuesta recibida a 300 baud: %s\n", id300.c_str());
    strncpy((char*)g_autoDetectDiag.signature, id300.c_str(), sizeof(g_autoDetectDiag.signature) - 1);

    if (id300.indexOf("HXE") >= 0 || id300.indexOf("HEX") >= 0) {
      debugPrintln("✅ [AUTO-DETECT] ¡Identificado Medidor Trifásico HEXING HXE34K!");
      g_autoDetectDiag.detectedModel = METER_MODEL_HEXING_HXE34K;
      g_autoDetectDiag.detectionStage = 4;
      strncpy((char*)g_autoDetectDiag.modelName, "Hexing HXE34K (Trifasico)", sizeof(g_autoDetectDiag.modelName) - 1);
      return METER_MODEL_HEXING_HXE34K;
    } else {
      debugPrintln("✅ [AUTO-DETECT] ¡Identificado Medidor Trifásico MIDDE DTS27!");
      g_autoDetectDiag.detectedModel = METER_MODEL_MIDDE_DTS27;
      g_autoDetectDiag.detectionStage = 4;
      strncpy((char*)g_autoDetectDiag.modelName, "MIDDE DTS27 (Trifasico)", sizeof(g_autoDetectDiag.modelName) - 1);
      return METER_MODEL_MIDDE_DTS27;
    }
  }

  // =========================================================================
  // FASE 3: SONDEO ACTIVO A 2400 BAUDIOS (MIDDE DDS26D)
  // =========================================================================
  debugPrintln("[AUTO-DETECT] Fase 3: Interrogación activa a 2400 baud (MIDDE DDS26D)...");
  g_autoDetectDiag.detectionStage = 3;

  delay(200);
  while (readCharBitbang7E1(rxPin, 30, BIT_TIME_US_2400) >= 0);

  sendStringBitbang7E1(txPin, "/?!\r\n", BIT_TIME_US_2400);

  String id2400 = "";
  unsigned long wait2400 = millis() + 2500;
  while (millis() < wait2400 && id2400.length() < 32) {
    int c = readCharBitbang7E1(rxPin, 300, BIT_TIME_US_2400);
    if (c >= 0) {
      char ch = (char)c;
      id2400 += ch;
      if (ch == '\n') break;
    } else if (id2400.length() > 0) {
      break;
    }
  }

  id2400.trim();
  if (id2400.length() > 0 && (id2400.startsWith("/") || id2400.indexOf("DDS") >= 0)) {
    debugPrintf("[AUTO-DETECT] Respuesta recibida a 2400 baud: %s\n", id2400.c_str());
    debugPrintln("✅ [AUTO-DETECT] ¡Identificado Medidor Monofásico MIDDE DDS26D!");
    g_autoDetectDiag.detectedModel = METER_MODEL_MIDDE_DDS26D;
    g_autoDetectDiag.detectionStage = 4;
    strncpy((char*)g_autoDetectDiag.modelName, "MIDDE DDS26D (Monofasico)", sizeof(g_autoDetectDiag.modelName) - 1);
    strncpy((char*)g_autoDetectDiag.signature, id2400.c_str(), sizeof(g_autoDetectDiag.signature) - 1);
    return METER_MODEL_MIDDE_DDS26D;
  }

  // =========================================================================
  // FASE 4: FALLBACK PREDETERMINADO
  // =========================================================================
  debugPrintln("⚠️ [AUTO-DETECT] Sin respuesta concluyente en sondeo. Estableciendo fallback: Hexing HXE34K.");
  g_autoDetectDiag.detectedModel = METER_MODEL_HEXING_HXE34K;
  g_autoDetectDiag.detectionStage = 5; // Fallback
  strncpy((char*)g_autoDetectDiag.modelName, "Hexing HXE34K (Fallback)", sizeof(g_autoDetectDiag.modelName) - 1);
  return METER_MODEL_HEXING_HXE34K;
}
