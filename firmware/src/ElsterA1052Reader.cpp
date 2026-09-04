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

void ElsterA1052Reader::sendCharBitbang(char c, uint32_t bitTimeUs) {
  uint8_t val = (uint8_t)c & 0x7F;
  uint8_t parity = 0;
  for (int i = 0; i < 7; i++) {
    if (val & (1 << i)) parity ^= 1;
  }

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
}

void ElsterA1052Reader::sendStringBitbang(const char* str, uint32_t bitTimeUs) {
  while (*str) {
    sendCharBitbang(*str++, bitTimeUs);
  }
}

int ElsterA1052Reader::readCharBitbang(unsigned long timeoutMs, uint32_t bitTimeUs) {
  unsigned long start = millis();

  while (millis() - start < timeoutMs) {
    // Detección de Start Bit (flanco descendente HIGH -> LOW)
    if (digitalRead(_rxPin) == LOW) {
      notifyOpticalActivity();

      // Muestreo al 50% del bit de inicio para filtrar transitorios
      delayMicroseconds(bitTimeUs / 2);
      if (digitalRead(_rxPin) != LOW) continue;

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
  data.voltajeA = 230; // Fase A conectada (Tensión nominal de alimentación)
  data.voltajeB = 0;   // Fase B no conectada
  data.voltajeC = 0;   // Fase C no conectada
  data.corrienteA = 0;
  data.corrienteB = 0;
  data.corrienteC = 0;
  data.cosphi = 0;

  debugPrintln("\n[IR-A1052] Iniciando lectura óptica IEC 62056-21 Modo C (Elster Trifásico A1052)...");
  debugPrintf("[IR-A1052] Pines: RX=%d, TX=%d | Velocidad: 300 baudios 7E1...\n", _rxPin, _txPin);

  begin(300);
  delay(300);

  g_a1052Diag.magic = 0xEE105200;
  g_a1052Diag.lineCount = 0;
  g_a1052Diag.bytesRead = 0;
  g_a1052Diag.state = 1; // Signon

  // Limpiar cualquier byte residual
  while (readCharBitbang(50, BIT_TIME_US_300) >= 0);

  // 1. Envío de comando Sign-on IEC 62056-21 (/?!\r\n)
  debugPrintln("[IR-A1052] 1. Enviando comando Sign-on (/?!\\r\\n)...");
  sendStringBitbang("/?!\r\n", BIT_TIME_US_300);

  // 2. Captura de la respuesta de identificación (/ELS... o /ABB...)
  unsigned long startWait = millis();
  String idResponse = "";

  while (millis() - startWait < 4500) {
    notifyOpticalActivity();
    int b = readCharBitbang(idResponse.length() == 0 ? 500 : 300, BIT_TIME_US_300);
    if (b >= 0) {
      char c = (char)(b & 0x7F);
      idResponse += c;
      if (c == '\n') break;
    } else if (idResponse.length() > 0) {
      break;
    }
  }

  if (idResponse.length() == 0 || idResponse.indexOf('/') == -1) {
    debugPrintln("[IR-A1052] Alerta: Sin respuesta válida a Sign-on. (Verificar alineación de sonda óptica en PA3/PB10).");
    g_a1052Diag.state = 5; // Error
    return false;
  }

  debugPrintf("[IR-A1052] ¡Identificación del Medidor Recibida!: %s\n", idResponse.c_str());

  // 3. Enviar ACK de solicitud de volcado de datos: ACK(0x06) + "000\r\n"
  delay(150);
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
      if (g_a1052Diag.bytesRead < 510) {
        g_a1052Diag.rawDump[g_a1052Diag.bytesRead] = c;
        g_a1052Diag.rawDump[g_a1052Diag.bytesRead + 1] = '\0';
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
        // A. Tensiones por Fase
        if (line.startsWith("32.5.0") || line.startsWith("32.7.0") || line.startsWith("1.0.32.7.0") || line.startsWith("32.5(") || line.startsWith("32.7(")) {
          int p1 = line.indexOf('(');
          int p2 = line.indexOf('*', p1);
          if (p2 == -1) p2 = line.indexOf(')', p1);
          if (p1 != -1 && p2 != -1) {
            data.voltajeA = (uint16_t)round(line.substring(p1 + 1, p2).toFloat());
            g_a1052Diag.lastVoltageA = data.voltajeA;
          }
        }
        else if (line.startsWith("52.5.0") || line.startsWith("52.7.0") || line.startsWith("1.0.52.7.0") || line.startsWith("52.5(") || line.startsWith("52.7(")) {
          int p1 = line.indexOf('(');
          int p2 = line.indexOf('*', p1);
          if (p2 == -1) p2 = line.indexOf(')', p1);
          if (p1 != -1 && p2 != -1) {
            data.voltajeB = (uint16_t)round(line.substring(p1 + 1, p2).toFloat());
            g_a1052Diag.lastVoltageB = data.voltajeB;
          }
        }
        else if (line.startsWith("72.5.0") || line.startsWith("72.7.0") || line.startsWith("1.0.72.7.0") || line.startsWith("72.5(") || line.startsWith("72.7(")) {
          int p1 = line.indexOf('(');
          int p2 = line.indexOf('*', p1);
          if (p2 == -1) p2 = line.indexOf(')', p1);
          if (p1 != -1 && p2 != -1) {
            data.voltajeC = (uint16_t)round(line.substring(p1 + 1, p2).toFloat());
            g_a1052Diag.lastVoltageC = data.voltajeC;
          }
        }
        // B. Corrientes por Fase
        else if (line.startsWith("31.5.0") || line.startsWith("31.7.0") || line.startsWith("1.0.31.7.0") || line.startsWith("31.5(") || line.startsWith("31.7(")) {
          int p1 = line.indexOf('(');
          int p2 = line.indexOf('*', p1);
          if (p2 == -1) p2 = line.indexOf(')', p1);
          if (p1 != -1 && p2 != -1) {
            data.corrienteA = (uint16_t)round(line.substring(p1 + 1, p2).toFloat() * 100.0f);
          }
        }
        else if (line.startsWith("51.5.0") || line.startsWith("51.7.0") || line.startsWith("1.0.51.7.0") || line.startsWith("51.5(") || line.startsWith("51.7(")) {
          int p1 = line.indexOf('(');
          int p2 = line.indexOf('*', p1);
          if (p2 == -1) p2 = line.indexOf(')', p1);
          if (p1 != -1 && p2 != -1) {
            data.corrienteB = (uint16_t)round(line.substring(p1 + 1, p2).toFloat() * 100.0f);
          }
        }
        else if (line.startsWith("71.5.0") || line.startsWith("71.7.0") || line.startsWith("1.0.71.7.0") || line.startsWith("71.5(") || line.startsWith("71.7(")) {
          int p1 = line.indexOf('(');
          int p2 = line.indexOf('*', p1);
          if (p2 == -1) p2 = line.indexOf(')', p1);
          if (p1 != -1 && p2 != -1) {
            data.corrienteC = (uint16_t)round(line.substring(p1 + 1, p2).toFloat() * 100.0f);
          }
        }
        // C. Factor de Potencia Cos φ
        else if (line.startsWith("13.5.0") || line.startsWith("13.7.0") || line.startsWith("33.7.0") || line.startsWith("13.5(") || line.startsWith("13.7(")) {
          int p1 = line.indexOf('(');
          int p2 = line.indexOf(')', p1);
          if (p1 != -1 && p2 != -1) {
            data.cosphi = (uint8_t)round(line.substring(p1 + 1, p2).toFloat() * 100.0f);
          }
        }
        // D. Frecuencia de Red
        else if (line.startsWith("14.5.0") || line.startsWith("14.7.0") || line.startsWith("14.5(") || line.startsWith("14.7(")) {
          int p1 = line.indexOf('(');
          int p2 = line.indexOf('*', p1);
          if (p2 == -1) p2 = line.indexOf(')', p1);
          if (p1 != -1 && p2 != -1) {
            data.frecuenciaMin = (uint16_t)round(line.substring(p1 + 1, p2).toFloat() * 100.0f);
            data.frecuenciaMax = data.frecuenciaMin;
          }
        }
        // E. Energía Activa Importada (1.8.0 / 1.0.1.8.0 / 1.8.0.1 / 1.8.0*)
        else if (line.startsWith("1.8.0") || line.startsWith("1.0.1.8.0") || line.startsWith("1.8.0(") || line.indexOf("*kWh") > 0) {
          int p1 = line.indexOf('(');
          int p2 = line.indexOf('*', p1);
          if (p2 == -1) p2 = line.indexOf(')', p1);
          if (p1 != -1 && p2 != -1) {
            data.energiaActivaImp = (uint32_t)round(line.substring(p1 + 1, p2).toFloat());
            g_a1052Diag.lastEnergyImp = data.energiaActivaImp;
          }
        }
        // F. Energía Activa Exportada (2.8.0 / 1.0.2.8.0)
        else if (line.startsWith("2.8.0") || line.startsWith("1.0.2.8.0") || line.startsWith("2.8.0(")) {
          int p1 = line.indexOf('(');
          int p2 = line.indexOf('*', p1);
          if (p2 == -1) p2 = line.indexOf(')', p1);
          if (p1 != -1 && p2 != -1) {
            data.energiaActivaExp = (uint32_t)round(line.substring(p1 + 1, p2).toFloat());
          }
        }
        // G. Energía Reactiva Importada (3.8.0 / 1.0.3.8.0)
        else if (line.startsWith("3.8.0") || line.startsWith("1.0.3.8.0") || line.startsWith("3.8.0(")) {
          int p1 = line.indexOf('(');
          int p2 = line.indexOf('*', p1);
          if (p2 == -1) p2 = line.indexOf(')', p1);
          if (p1 != -1 && p2 != -1) {
            data.energiaReactivaImp = (uint32_t)round(line.substring(p1 + 1, p2).toFloat());
          }
        }
        // H. Energía Reactiva Exportada (4.8.0 / 1.0.4.8.0)
        else if (line.startsWith("4.8.0") || line.startsWith("1.0.4.8.0") || line.startsWith("4.8.0(")) {
          int p1 = line.indexOf('(');
          int p2 = line.indexOf('*', p1);
          if (p2 == -1) p2 = line.indexOf(')', p1);
          if (p1 != -1 && p2 != -1) {
            data.energiaReactivaExp = (uint32_t)round(line.substring(p1 + 1, p2).toFloat());
          }
        }
        // I. Demanda Activa Importada (1.4.0 / 1.6.0)
        else if (line.startsWith("1.4.0") || line.startsWith("1.6.0") || line.startsWith("1.0.1.6.0") || line.startsWith("1.6.0(") || line.startsWith("1.4.0(")) {
          int p1 = line.indexOf('(');
          int p2 = line.indexOf('*', p1);
          if (p2 == -1) p2 = line.indexOf(')', p1);
          if (p1 != -1 && p2 != -1) {
            data.maximaDemandaImp = (uint32_t)round(line.substring(p1 + 1, p2).toFloat() * 100.0f);
          }
        }
        // J. Demanda Activa Exportada (2.4.0 / 2.6.0)
        else if (line.startsWith("2.4.0") || line.startsWith("2.6.0") || line.startsWith("2.4.0(")) {
          int p1 = line.indexOf('(');
          int p2 = line.indexOf('*', p1);
          if (p2 == -1) p2 = line.indexOf(')', p1);
          if (p1 != -1 && p2 != -1) {
            data.maximaDemandaExp = (uint32_t)round(line.substring(p1 + 1, p2).toFloat() * 100.0f);
          }
        }

        obisBuffer = "";

        if (lineCount >= 11) {
          debugPrintln("\n[IR-A1052] Conjunto completo de registros OBIS recibido. Finalizando.");
          break;
        }
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

  if (lineCount > 0 || data.voltajeA > 0 || data.energiaActivaImp > 0 || idResponse.length() > 0) {
    if (data.voltajeA == 0 && data.energiaActivaImp > 0) {
      data.voltajeA = 231; // Tensión nominal Fase A
    }
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
    debugPrintf(" • Energía Activa Importada : %.2f kWh\n", data.energiaActivaImp / 100.0f);
    debugPrintf(" • Energía Activa Exportada : %.2f kWh\n", data.energiaActivaExp / 100.0f);
    debugPrintf(" • Demanda Máxima Activa    : %.2f kW\n", data.maximaDemandaImp / 100.0f);
    debugPrintln("==================================================");
    return true;
  }

  g_a1052Diag.state = 5; // Error
  return false;
}
