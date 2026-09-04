#include "ElsterA1052Reader.h"
#include "MeterConfig.h"
#include "BoardConfig.h"
#include "DebugSerial.h"

// 300 baudios -> 1 bit = 1000000 / 300 = 3333.33 us
#define BIT_TIME_US_300 3333

// Estructura de Diagnóstico en RAM para lectura SWD/OpenOCD
struct __attribute__((packed)) ElsterA1052Diag {
  uint32_t magic;         // 0xEE105200
  uint16_t lineCount;
  uint16_t bytesRead;
  uint16_t lastVoltageA;
  uint16_t lastVoltageB;
  uint16_t lastVoltageC;
  uint32_t lastEnergyImp;
  uint8_t  state;         // 0=idle, 1=signon, 2=ack_sent, 3=reading_obis, 4=success, 5=error
  uint8_t  padding[3];
  char     rawDump[512];
};

volatile ElsterA1052Diag g_a1052Diag = {0xEE105200, 0, 0, 0, 0, 0, 0, 0, {0}, {0}};

ElsterA1052Reader::ElsterA1052Reader(uint8_t rxPin, uint8_t txPin) 
  : BaseMeterReader(rxPin, txPin) {}

void ElsterA1052Reader::begin(unsigned long baudRate) {
  pinMode(_txPin, OUTPUT);
  digitalWrite(_txPin, HIGH); // Reposo HIGH para puerto óptico IR
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

void ElsterA1052Reader::sendCharBitbang(char c, uint32_t bitTimeUs) {
  uint8_t val = (uint8_t)c & 0x7F;
  uint8_t parity = 0;
  for (int i = 0; i < 7; i++) {
    if (val & (1 << i)) parity ^= 1;
  }

  noInterrupts();
  // Start bit: LOW
  digitalWrite(_txPin, LOW);
  delayMicroseconds(bitTimeUs);

  // 7 Data bits (LSB first)
  for (int i = 0; i < 7; i++) {
    digitalWrite(_txPin, (val & (1 << i)) ? HIGH : LOW);
    delayMicroseconds(bitTimeUs);
  }

  // Even Parity bit
  digitalWrite(_txPin, parity ? HIGH : LOW);
  delayMicroseconds(bitTimeUs);

  // Stop bit: HIGH
  digitalWrite(_txPin, HIGH);
  delayMicroseconds(bitTimeUs * 2); // 2 stop bits para robustez
  interrupts();
}

void ElsterA1052Reader::sendStringBitbang(const char* str, uint32_t bitTimeUs) {
  while (*str) {
    sendCharBitbang(*str++, bitTimeUs);
  }
}

int ElsterA1052Reader::readCharBitbang(unsigned long timeoutMs, uint32_t bitTimeUs) {
  unsigned long start = millis();

  while (millis() - start < timeoutMs) {
    // Si la línea está baja al inicio, esperar a que vuelva a HIGH (IDLE)
    if (digitalRead(_rxPin) == LOW) {
      unsigned long waitIdle = micros();
      while (digitalRead(_rxPin) == LOW && (micros() - waitIdle < bitTimeUs * 3)) {
      }
      if (digitalRead(_rxPin) == LOW) {
        continue;
      }
    }

    // Aguardar flanco descendente (Start Bit HIGH -> LOW)
    if (digitalRead(_rxPin) == LOW) {
      notifyOpticalActivity();

      noInterrupts();
      // Muestreo al 50% del bit de inicio para filtrar transitorios
      delayMicroseconds(bitTimeUs / 2);
      if (digitalRead(_rxPin) != LOW) {
        interrupts();
        continue;
      }

      uint8_t val = 0;
      for (int i = 0; i < 7; i++) {
        delayMicroseconds(bitTimeUs);
        if (digitalRead(_rxPin) == HIGH) {
          val |= (1 << i);
        }
      }

      // Saltar bit de paridad y avanzar al bit de parada
      delayMicroseconds(bitTimeUs);
      delayMicroseconds(bitTimeUs);
      interrupts();

      // Aguardar línea en reposo (HIGH)
      unsigned long waitStop = micros();
      while (digitalRead(_rxPin) == LOW && (micros() - waitStop < bitTimeUs * 2)) {
      }

      return val & 0x7F;
    }
  }
  return -1;
}

bool ElsterA1052Reader::performOpticalRead(MeterData &data, unsigned long timeoutMs) {
  data.tipoMedidor = 2; // 2 = Trifásico (Elster A1052)
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

  debugPrintln("\n[IR-A1052] Iniciando lectura óptica IEC 62056-21 Modo C (Elster Trifásico A1052)...");
  debugPrintf("[IR-A1052] Pines: RX=%d, TX=%d | Velocidad: 300 baudios 7E1...\n", _rxPin, _txPin);

  begin(300);
  delay(300);

  g_a1052Diag.magic = 0xEE105200;
  g_a1052Diag.lineCount = 0;
  g_a1052Diag.bytesRead = 0;
  g_a1052Diag.lastVoltageA = 0;
  g_a1052Diag.lastVoltageB = 0;
  g_a1052Diag.lastVoltageC = 0;
  g_a1052Diag.lastEnergyImp = 0;
  g_a1052Diag.state = 1; // Signon
  memset((char*)g_a1052Diag.rawDump, 0, sizeof(g_a1052Diag.rawDump));

  // Limpiar cualquier byte residual
  while (readCharBitbang(50, BIT_TIME_US_300) >= 0);

  // 1. Envío de comando Sign-on IEC 62056-21 (/?!\r\n) con hasta 2 reintentos
  String idResponse = "";
  for (int attempt = 0; attempt < 2; attempt++) {
    debugPrintf("[IR-A1052] Intento %d: Enviando comando Sign-on (/?!\\r\\n)...\n", attempt + 1);
    while (readCharBitbang(50, BIT_TIME_US_300) >= 0); // Limpiar buffer
    sendStringBitbang("/?!\r\n", BIT_TIME_US_300);

    unsigned long startWait = millis();
    idResponse = "";

    while (millis() - startWait < 4500) {
      notifyOpticalActivity();
      int b = readCharBitbang(idResponse.length() == 0 ? 600 : 350, BIT_TIME_US_300);
      if (b >= 0) {
        char c = (char)(b & 0x7F);
        if (c >= 32 && c <= 126) {
          idResponse += c;
        } else if (c == '\r' || c == '\n') {
          if (idResponse.length() > 0) break;
        }
      } else if (idResponse.length() > 0) {
        break;
      }
    }

    if (idResponse.length() > 0 && idResponse.indexOf('/') >= 0) {
      break;
    }
    delay(1000);
  }

  snprintf((char*)g_a1052Diag.rawDump, sizeof(g_a1052Diag.rawDump), "ID: %s\r\n", idResponse.c_str());
  int dumpOffset = strlen((char*)g_a1052Diag.rawDump);

  if (idResponse.length() == 0) {
    debugPrintln("[IR-A1052] Alerta: Sin respuesta a Sign-on. (Verificar alineación de sonda óptica en PA3/PB10).");
    g_a1052Diag.state = 5; // Error
    return false;
  }

  debugPrintf("[IR-A1052] ¡Identificación del Medidor Recibida!: %s\n", idResponse.c_str());

  // 3. Enviar ACK de solicitud de volcado de datos: ACK(0x06) + "000\r\n"
  delay(200);
  debugPrintln("[IR-A1052] 2. Enviando ACK (\\x06000\\r\\n) para solicitar registros OBIS...");
  sendCharBitbang(0x06, BIT_TIME_US_300);
  sendStringBitbang("000\r\n", BIT_TIME_US_300);

  g_a1052Diag.state = 3; // Reading OBIS

  // 4. Captura y parseo del bloque de registros OBIS
  debugPrintln("[IR-A1052] 3. Recibiendo y decodificando registros OBIS:");
  unsigned long startObis = millis();
  unsigned long hardDeadline = millis() + timeoutMs;
  unsigned long lastCharTime = millis();
  String obisBuffer = "";
  int lineCount = 0;

  while (millis() < hardDeadline) {
    notifyOpticalActivity();
    int b = readCharBitbang(400, BIT_TIME_US_300);
    if (b >= 0) {
      char c = (char)(b & 0x7F);
      debugPrintChar(c);
      obisBuffer += c;
      if (dumpOffset < 500) {
        g_a1052Diag.rawDump[dumpOffset++] = c;
        g_a1052Diag.rawDump[dumpOffset] = '\0';
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

        // Parseo flexible e independiente de registros OBIS del medidor Elster A1052
        // A. Tensiones por Fase (L1/A = 32.x, L2/B = 52.x, L3/C = 72.x)
        if (line.indexOf("32.7") >= 0 || line.indexOf("32.5") >= 0 || line.indexOf("32.25") >= 0 || line.indexOf("32.8") >= 0 || line.indexOf("U1(") >= 0 || line.indexOf("V1(") >= 0 || line.indexOf("VA(") >= 0 || line.indexOf("UL1(") >= 0) {
          data.voltajeA = (uint16_t)round(extractObisValue(line));
          g_a1052Diag.lastVoltageA = data.voltajeA;
        }
        else if (line.indexOf("52.7") >= 0 || line.indexOf("52.5") >= 0 || line.indexOf("52.25") >= 0 || line.indexOf("52.8") >= 0 || line.indexOf("U2(") >= 0 || line.indexOf("V2(") >= 0 || line.indexOf("VB(") >= 0 || line.indexOf("UL2(") >= 0) {
          data.voltajeB = (uint16_t)round(extractObisValue(line));
          g_a1052Diag.lastVoltageB = data.voltajeB;
        }
        else if (line.indexOf("72.7") >= 0 || line.indexOf("72.5") >= 0 || line.indexOf("72.25") >= 0 || line.indexOf("72.8") >= 0 || line.indexOf("U3(") >= 0 || line.indexOf("V3(") >= 0 || line.indexOf("VC(") >= 0 || line.indexOf("UL3(") >= 0) {
          data.voltajeC = (uint16_t)round(extractObisValue(line));
          g_a1052Diag.lastVoltageC = data.voltajeC;
        }
        // B. Corrientes por Fase (L1/A = 31.x, L2/B = 51.x, L3/C = 71.x)
        else if (line.indexOf("31.7") >= 0 || line.indexOf("31.5") >= 0 || line.indexOf("31.25") >= 0 || line.indexOf("I1(") >= 0 || line.indexOf("IA(") >= 0 || line.indexOf("IL1(") >= 0) {
          data.corrienteA = (uint16_t)round(extractObisValue(line) * 100.0f);
        }
        else if (line.indexOf("51.7") >= 0 || line.indexOf("51.5") >= 0 || line.indexOf("51.25") >= 0 || line.indexOf("I2(") >= 0 || line.indexOf("IB(") >= 0 || line.indexOf("IL2(") >= 0) {
          data.corrienteB = (uint16_t)round(extractObisValue(line) * 100.0f);
        }
        else if (line.indexOf("71.7") >= 0 || line.indexOf("71.5") >= 0 || line.indexOf("71.25") >= 0 || line.indexOf("I3(") >= 0 || line.indexOf("IC(") >= 0 || line.indexOf("IL3(") >= 0) {
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
        else if (line.indexOf("1.8.0") >= 0 || line.indexOf("1.8.1") >= 0 || line.indexOf("*kWh") > 0) {
          data.energiaActivaImp = (uint32_t)round(extractObisValue(line));
          g_a1052Diag.lastEnergyImp = data.energiaActivaImp;
        }
        // F. Energía Activa Exportada (2.8.0 / 1.0.2.8.0)
        else if (line.indexOf("2.8.0") >= 0 || line.indexOf("2.8.1") >= 0) {
          data.energiaActivaExp = (uint32_t)round(extractObisValue(line));
        }
        // G. Energía Reactiva Importada (3.8.0 / 1.0.3.8.0)
        else if (line.indexOf("3.8.0") >= 0 || line.indexOf("3.8.1") >= 0) {
          data.energiaReactivaImp = (uint32_t)round(extractObisValue(line));
        }
        // H. Energía Reactiva Exportada (4.8.0 / 1.0.4.8.0)
        else if (line.indexOf("4.8.0") >= 0 || line.indexOf("4.8.1") >= 0) {
          data.energiaReactivaExp = (uint32_t)round(extractObisValue(line));
        }
        // I. Demanda Activa Importada (1.4.0 / 1.6.0)
        else if (line.indexOf("1.4.0") >= 0 || line.indexOf("1.6.0") >= 0) {
          data.maximaDemandaImp = (uint32_t)round(extractObisValue(line) * 100.0f);
        }
        // J. Demanda Activa Exportada (2.4.0 / 2.6.0)
        else if (line.indexOf("2.4.0") >= 0 || line.indexOf("2.6.0") >= 0) {
          data.maximaDemandaExp = (uint32_t)round(extractObisValue(line) * 100.0f);
        }

        obisBuffer = "";
      }

      if (c == '!' || c == 0x03) { // Fin de bloque IEC 62056-21
        debugPrintln("\n[IR-A1052] Fin de bloque detectado (! / ETX).");
        break;
      }
    }

    if (lineCount == 0 && (millis() - startObis > 5000)) {
      debugPrintln("\n[IR-A1052] Sin líneas OBIS válidas recibidas.");
      break;
    }

    if (lineCount >= 3 && (millis() - lastCharTime > 1500)) {
      debugPrintln("\n[IR-A1052] Fin de flujo OBIS por silencio de línea.");
      break;
    }
  }

  // Validación estricta: sólo considerar lectura exitosa si hay líneas OBIS o valores de energía/tensión reales
  if (lineCount >= 3 || data.voltajeA > 0 || data.voltajeB > 0 || data.voltajeC > 0 || data.energiaActivaImp > 0) {
    g_a1052Diag.state = 4; // Success
    data.lecturaValida = true;
    debugPrintln("\n==================================================");
    debugPrintln("=== ¡¡¡PARÁMETROS DECODIFICADOS ELSTER A1052!!! ===");
    debugPrintln("==================================================");
    debugPrintf(" • ID Medidor               : %s\n", idResponse.c_str());
    debugPrintf(" • Voltaje Fase A           : %u V\n", data.voltajeA);
    debugPrintf(" • Voltaje Fase B           : %u V\n", data.voltajeB);
    debugPrintf(" • Voltaje Fase C           : %u V\n", data.voltajeC);
    debugPrintf(" • Corriente Fase A         : %.2f A\n", data.corrienteA / 100.0f);
    debugPrintf(" • Corriente Fase B         : %.2f A\n", data.corrienteB / 100.0f);
    debugPrintf(" • Corriente Fase C         : %.2f A\n", data.corrienteC / 100.0f);
    debugPrintf(" • Factor de Potencia (Cosφ): %.2f\n", data.cosphi / 100.0f);
    debugPrintf(" • Frecuencia Red           : %.2f Hz\n", data.frecuenciaMin / 100.0f);
    debugPrintf(" • Energía Activa Importada : %lu kWh\n", (unsigned long)data.energiaActivaImp);
    debugPrintf(" • Energía Activa Exportada : %lu kWh\n", (unsigned long)data.energiaActivaExp);
    debugPrintf(" • Demanda Máxima Activa    : %.2f kW\n", data.maximaDemandaImp / 100.0f);
    debugPrintln("==================================================");
    return true;
  }

  g_a1052Diag.state = 5; // Error
  return false;
}
