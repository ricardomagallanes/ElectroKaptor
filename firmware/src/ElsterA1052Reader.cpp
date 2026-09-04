#include "ElsterA1052Reader.h"
#include "MeterConfig.h"
#include "BoardConfig.h"
#include "DebugSerial.h"

#define BIT_TIME_US_2400 417

// Estructura de Diagnóstico en RAM para lectura SWD/OpenOCD
struct __attribute__((packed)) ElsterA1052Diag {
  uint32_t magic;         // 0xEE105200
  uint16_t lineCount;
  uint16_t bytesRead;
  uint16_t lastVoltageA;
  uint16_t lastVoltageB;
  uint16_t lastVoltageC;
  uint32_t lastEnergyImp;
  uint8_t  state;         // 1=reading, 4=success, 5=error
  uint8_t  padding[3];
  char     rawDump[512];
};

volatile ElsterA1052Diag g_a1052Diag = {0xEE105200, 0, 0, 0, 0, 0, 0, 0, {0}, {0}};

ElsterA1052Reader::ElsterA1052Reader(uint8_t rxPin, uint8_t txPin) 
  : BaseMeterReader(rxPin, txPin) {}

void ElsterA1052Reader::begin(unsigned long baudRate) {
  pinMode(_txPin, OUTPUT);
  digitalWrite(_txPin, HIGH);
  pinMode(_rxPin, INPUT_PULLUP);
}

static float extractObisValue(const String &line) {
  int p1 = line.indexOf('(');
  if (p1 == -1) return 0.0f;
  int p2 = line.indexOf('*', p1);
  if (p2 == -1) p2 = line.indexOf(')', p1);
  if (p2 == -1) return 0.0f;
  return line.substring(p1 + 1, p2).toFloat();
}

int ElsterA1052Reader::readByteFast2400(uint32_t timeoutUs) {
  uint32_t startWait = micros();

  // Aguardar flanco descendente (Start bit IR pulse)
  while (digitalRead(_rxPin) == HIGH) {
    if ((uint32_t)(micros() - startWait) > timeoutUs) {
      return -1; // Timeout
    }
  }

  notifyOpticalActivity();
  uint32_t t0 = micros();
  uint8_t rawVal = 0;

  // Muestrear los 8 slots de bits a 2400 baudios (417 us por slot)
  for (int i = 0; i < 8; i++) {
    uint32_t slotStart = t0 + (i * BIT_TIME_US_2400) + 180;
    uint32_t slotEnd   = t0 + ((i + 1) * BIT_TIME_US_2400) + 180;

    while ((int32_t)(micros() - slotStart) < 0);

    bool pulseFound = false;
    while ((int32_t)(micros() - slotEnd) < 0) {
      if (digitalRead(_rxPin) == LOW) {
        pulseFound = true;
      }
    }

    if (pulseFound) {
      rawVal |= (1 << i);
    }
  }

  uint32_t charEnd = t0 + (10 * BIT_TIME_US_2400);
  while ((int32_t)(micros() - charEnd) < 0);

  // Inversión lógica de modulación IR: pulso LOW = bit 0
  uint8_t val = (~rawVal) & 0xFF;
  return (int)val;
}

void ElsterA1052Reader::sendCharBitbang(char c, uint32_t bitTimeUs) {
  uint8_t val = (uint8_t)c & 0x7F;
  uint8_t parity = 0;
  for (int i = 0; i < 7; i++) {
    if (val & (1 << i)) parity ^= 1;
  }

  digitalWrite(_txPin, LOW);
  delayMicroseconds(bitTimeUs);

  for (int i = 0; i < 7; i++) {
    digitalWrite(_txPin, (val & (1 << i)) ? HIGH : LOW);
    delayMicroseconds(bitTimeUs);
  }

  digitalWrite(_txPin, parity ? HIGH : LOW);
  delayMicroseconds(bitTimeUs);

  digitalWrite(_txPin, HIGH);
  delayMicroseconds(bitTimeUs * 2);
}

void ElsterA1052Reader::sendStringBitbang(const char* str, uint32_t bitTimeUs) {
  while (*str) {
    sendCharBitbang(*str++, bitTimeUs);
  }
}

int ElsterA1052Reader::readCharBitbang(unsigned long timeoutMs, uint32_t bitTimeUs) {
  return readByteFast2400(timeoutMs * 1000);
}

bool ElsterA1052Reader::performOpticalRead(MeterData &data, unsigned long timeoutMs) {
  data.tipoMedidor = 2; // Trifásico (Elster A1052)
  data.bateria = 36;
  data.temperatura = 25;
  data.frecuenciaMin = 5000;
  data.frecuenciaMax = 5000;
  data.voltajeA = 0;
  data.voltajeB = 0;
  data.voltajeC = 0;
  data.corrienteA = 0;
  data.corrienteB = 0;
  data.corrienteC = 0;
  data.cosphi = 0;
  data.lecturaValida = false;

  debugPrintln("\n[IR-A1052] Capturando ráfaga óptica OBIS Elster Trifásico A1052 (2400 baudios)...");
  debugPrintf("[IR-A1052] Pines: RX=%d, TX=%d\n", _rxPin, _txPin);

  begin(2400);

  g_a1052Diag.magic = 0xEE105200;
  g_a1052Diag.lineCount = 0;
  g_a1052Diag.bytesRead = 0;
  g_a1052Diag.lastVoltageA = 0;
  g_a1052Diag.lastVoltageB = 0;
  g_a1052Diag.lastVoltageC = 0;
  g_a1052Diag.lastEnergyImp = 0;
  g_a1052Diag.state = 1; // Reading
  memset((char*)g_a1052Diag.rawDump, 0, sizeof(g_a1052Diag.rawDump));

  // Limpiar residuos
  while (readByteFast2400(2000) >= 0);

  unsigned long hardDeadline = millis() + timeoutMs;
  unsigned long lastCharTime = millis();
  String obisBuffer = "";
  int lineCount = 0;
  int dumpIdx = 0;

  while (millis() < hardDeadline) {
    notifyOpticalActivity();
    int b = readByteFast2400(3500000); // hasta 3.5s entre caracteres/ráfagas
    if (b >= 0) {
      char c = (char)(b & 0x7F);
      debugPrintChar(c);
      obisBuffer += c;

      if (dumpIdx < sizeof(g_a1052Diag.rawDump) - 2) {
        g_a1052Diag.rawDump[dumpIdx++] = c;
        g_a1052Diag.rawDump[dumpIdx] = '\0';
      }
      g_a1052Diag.bytesRead++;

      if ((c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '.' || c == '(' || c == ')' || c == '*') {
        lastCharTime = millis();
      }

      if (c == '\n') {
        String line = obisBuffer;
        line.trim();
        while (line.length() > 0 && (uint8_t)line[0] < 0x20) {
          line.remove(0, 1);
        }
        lineCount++;
        g_a1052Diag.lineCount = lineCount;

        // Parseo de registros OBIS del medidor Elster A1052
        // A. Tensiones por Fase (L1/A = 32.x, L2/B = 52.x, L3/C = 72.x)
        if (line.indexOf("32.7") >= 0 || line.indexOf("32.5") >= 0 || line.indexOf("32.25") >= 0 || line.indexOf("U1(") >= 0 || line.indexOf("V1(") >= 0 || line.indexOf("VA(") >= 0 || line.indexOf("UL1(") >= 0) {
          data.voltajeA = (uint16_t)round(extractObisValue(line));
          g_a1052Diag.lastVoltageA = data.voltajeA;
        }
        else if (line.indexOf("52.7") >= 0 || line.indexOf("52.5") >= 0 || line.indexOf("52.25") >= 0 || line.indexOf("U2(") >= 0 || line.indexOf("V2(") >= 0 || line.indexOf("VB(") >= 0 || line.indexOf("UL2(") >= 0) {
          data.voltajeB = (uint16_t)round(extractObisValue(line));
          g_a1052Diag.lastVoltageB = data.voltajeB;
        }
        else if (line.indexOf("72.7") >= 0 || line.indexOf("72.5") >= 0 || line.indexOf("72.25") >= 0 || line.indexOf("U3(") >= 0 || line.indexOf("V3(") >= 0 || line.indexOf("VC(") >= 0 || line.indexOf("UL3(") >= 0) {
          data.voltajeC = (uint16_t)round(extractObisValue(line));
          g_a1052Diag.lastVoltageC = data.voltajeC;
        }
        // B. Corrientes por Fase (L1/A = 31.x, L2/B = 51.x, L3/C = 71.x)
        else if (line.indexOf("31.7") >= 0 || line.indexOf("31.5") >= 0 || line.indexOf("I1(") >= 0 || line.indexOf("IA(") >= 0 || line.indexOf("IL1(") >= 0) {
          data.corrienteA = (uint16_t)round(extractObisValue(line) * 100.0f);
        }
        else if (line.indexOf("51.7") >= 0 || line.indexOf("51.5") >= 0 || line.indexOf("I2(") >= 0 || line.indexOf("IB(") >= 0 || line.indexOf("IL2(") >= 0) {
          data.corrienteB = (uint16_t)round(extractObisValue(line) * 100.0f);
        }
        else if (line.indexOf("71.7") >= 0 || line.indexOf("71.5") >= 0 || line.indexOf("I3(") >= 0 || line.indexOf("IC(") >= 0 || line.indexOf("IL3(") >= 0) {
          data.corrienteC = (uint16_t)round(extractObisValue(line) * 100.0f);
        }
        // C. Factor de Potencia Cos φ
        else if (line.indexOf("13.7") >= 0 || line.indexOf("13.5") >= 0 || line.indexOf("33.7") >= 0 || line.indexOf("PF(") >= 0) {
          data.cosphi = (uint8_t)round(extractObisValue(line) * 100.0f);
        }
        // D. Frecuencia de Red
        else if (line.indexOf("14.7") >= 0 || line.indexOf("14.5") >= 0 || line.indexOf("FREQ(") >= 0 || line.indexOf("HZ(") >= 0) {
          data.frecuenciaMin = (uint16_t)round(extractObisValue(line) * 100.0f);
          data.frecuenciaMax = data.frecuenciaMin;
        }
        // E. Energía Activa Importada (1.8.0 / 1.0.1.8.0 / 1.8.0.1 / 1.8.0*)
        else if (line.indexOf("1.8.0") >= 0 || line.indexOf(".8.0") >= 0 || line.indexOf("*kWh") > 0) {
          float kwh = extractObisValue(line);
          if (kwh > 0.0f) {
            data.energiaActivaImp = (uint32_t)round(kwh * 100.0f);
            g_a1052Diag.lastEnergyImp = data.energiaActivaImp;
          }
        }
        // F. Energía Activa Exportada (2.8.0 / 1.0.2.8.0)
        else if (line.indexOf("2.8.0") >= 0 || line.indexOf("2.8.1") >= 0) {
          data.energiaActivaExp = (uint32_t)round(extractObisValue(line) * 100.0f);
        }
        // G. Energía Reactiva Importada (3.8.0 / 1.0.3.8.0)
        else if (line.indexOf("3.8.0") >= 0 || line.indexOf("3.8.1") >= 0 || line.indexOf("*kVArh") > 0) {
          data.energiaReactivaImp = (uint32_t)round(extractObisValue(line) * 100.0f);
        }
        // H. Energía Reactiva Exportada (4.8.0 / 1.0.4.8.0)
        else if (line.indexOf("4.8.0") >= 0 || line.indexOf("4.8.1") >= 0) {
          data.energiaReactivaExp = (uint32_t)round(extractObisValue(line) * 100.0f);
        }
        // I. Demanda Activa Importada (1.4.0 / 1.6.0 / 1.2.0)
        else if (line.indexOf("1.4.0") >= 0 || line.indexOf("1.6.0") >= 0 || line.indexOf("1.2.0") >= 0 || line.indexOf("*kW") > 0) {
          data.maximaDemandaImp = (uint32_t)round(extractObisValue(line) * 100.0f);
        }
        // J. Demanda Activa Exportada (2.4.0 / 2.6.0)
        else if (line.indexOf("2.4.0") >= 0 || line.indexOf("2.6.0") >= 0) {
          data.maximaDemandaExp = (uint32_t)round(extractObisValue(line) * 100.0f);
        }

        obisBuffer = "";
      }

      if (c == '!' || c == 0x03) {
        debugPrintln("\n[IR-A1052] Fin de bloque detectado (! / ETX).");
        break;
      }
    } else {
      // Timeout inter-byte
      if (lineCount >= 4) {
        break;
      }
    }

    if (lineCount >= 6 && (millis() - lastCharTime > 1500)) {
      debugPrintln("\n[IR-A1052] Fin de ráfaga por silencio.");
      break;
    }
  }

  if (lineCount >= 3 || data.voltajeA > 0 || data.voltajeB > 0 || data.voltajeC > 0 || data.energiaActivaImp > 0) {
    g_a1052Diag.state = 4; // Success
    data.lecturaValida = true;

    // Si el medidor reporta energía activa válida pero no incluye registros instantáneos por fase en el flujo óptico:
    // Asignar tensión nominal estándar en Fase A (igual que el equipo original)
    if (data.voltajeA == 0 && data.voltajeB == 0 && data.voltajeC == 0 && data.energiaActivaImp > 0) {
      data.voltajeA = 230;
      data.voltajeB = 0;
      data.voltajeC = 0;
    }

    g_a1052Diag.lastVoltageA = data.voltajeA;
    g_a1052Diag.lastVoltageB = data.voltajeB;
    g_a1052Diag.lastVoltageC = data.voltajeC;

    debugPrintln("\n==================================================");
    debugPrintln("=== ¡¡¡PARÁMETROS DECODIFICADOS ELSTER A1052!!! ===");
    debugPrintln("==================================================");
    debugPrintf(" • Voltaje Fase A           : %u V\n", data.voltajeA);
    debugPrintf(" • Voltaje Fase B           : %u V\n", data.voltajeB);
    debugPrintf(" • Voltaje Fase C           : %u V\n", data.voltajeC);
    debugPrintf(" • Corriente Fase A         : %.2f A\n", data.corrienteA / 100.0f);
    debugPrintf(" • Corriente Fase B         : %.2f A\n", data.corrienteB / 100.0f);
    debugPrintf(" • Corriente Fase C         : %.2f A\n", data.corrienteC / 100.0f);
    debugPrintf(" • Factor de Potencia (Cosφ): %.2f\n", data.cosphi / 100.0f);
    debugPrintf(" • Energía Activa Importada : %lu kWh\n", (unsigned long)data.energiaActivaImp);
    debugPrintf(" • Energía Reactiva Import. : %lu kVARh\n", (unsigned long)data.energiaReactivaImp);
    debugPrintf(" • Demanda Máxima Activa    : %.2f kW\n", data.maximaDemandaImp / 100.0f);
    debugPrintln("==================================================");
    return true;
  }

  g_a1052Diag.state = 5; // Error
  return false;
}
