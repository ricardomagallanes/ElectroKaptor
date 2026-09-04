#include "MiddeDDS26DReader.h"
#include "MeterConfig.h"
#include "BoardConfig.h"
#include "DebugSerial.h"

// Variables de diagnóstico en RAM para inspección con OpenOCD / GDB
char g_dds26RawDump[1024] = {0};
static uint16_t g_dds26DumpIdx = 0;
char g_dds26Id[64] = {0};
uint16_t g_dds26LineCount = 0;
uint32_t g_dds26EdgeCount = 0;
uint8_t  g_dds26LastByte = 0;
uint8_t  g_dds26PinState = 1;
char g_dds26ActiveMode[32] = "None";

MiddeDDS26DReader::MiddeDDS26DReader(uint8_t rxPin, uint8_t txPin) 
  : _rxPin(rxPin), _txPin(txPin) {}

void MiddeDDS26DReader::begin(unsigned long baudRate) {
  pinMode(_txPin, OUTPUT);
  digitalWrite(_txPin, HIGH); // Reposo HIGH
  pinMode(_rxPin, INPUT_PULLUP);
}

void MiddeDDS26DReader::sendChar(char c, uint32_t bitTimeUs, bool is7E1) {
  uint8_t val = (uint8_t)c;
  uint8_t bits = is7E1 ? 7 : 8;
  uint8_t parity = 0;
  
  if (is7E1) {
    val &= 0x7F;
    for (int i = 0; i < 7; i++) {
      if (val & (1 << i)) parity ^= 1;
    }
  }

  // Start bit: LOW
  digitalWrite(_txPin, LOW);
  delayMicroseconds(bitTimeUs);

  // Data bits (LSB first)
  for (int i = 0; i < bits; i++) {
    digitalWrite(_txPin, (val & (1 << i)) ? HIGH : LOW);
    delayMicroseconds(bitTimeUs);
  }

  // Parity bit si es 7E1
  if (is7E1) {
    digitalWrite(_txPin, parity ? HIGH : LOW);
    delayMicroseconds(bitTimeUs);
  }

  // Stop bit: HIGH (1 stop bit)
  digitalWrite(_txPin, HIGH);
  delayMicroseconds(bitTimeUs);
}

void MiddeDDS26DReader::sendString(const char* str, uint32_t bitTimeUs, bool is7E1) {
  while (*str) {
    sendChar(*str++, bitTimeUs, is7E1);
  }
}

void MiddeDDS26DReader::sendIecBlock(char cmd, char param, const char* body, uint32_t bitTimeUs) {
  uint8_t bcc = (uint8_t)cmd ^ (uint8_t)param ^ 0x02; // SOH excluido del BCC según IEC 62056-21
  sendChar(0x01, bitTimeUs, true); // SOH
  sendChar(cmd, bitTimeUs, true);
  sendChar(param, bitTimeUs, true);
  sendChar(0x02, bitTimeUs, true); // STX
  
  const char* p = body;
  while (*p) {
    bcc ^= (uint8_t)(*p);
    sendChar(*p++, bitTimeUs, true);
  }
  
  bcc ^= 0x03; // ETX
  sendChar(0x03, bitTimeUs, true); // ETX
  sendChar((char)(bcc & 0x7F), bitTimeUs, true); // BCC (7E1)
}

bool MiddeDDS26DReader::readRegister(const char* obisCode, String &outVal, uint32_t bitTimeUs) {
  outVal = "";
  delay(150);
  sendIecBlock('R', '1', obisCode, bitTimeUs);
  
  unsigned long start = millis();
  String resp = "";
  while (millis() - start < 2000) {
    int b = readChar(bitTimeUs, true, resp.length() == 0 ? 800 : 150);
    if (b >= 0) {
      char c = (char)(b & 0x7F);
      if (c == '\x02') { // STX
        resp = "";
      } else if (c == '\x03') { // ETX
        break;
      } else if (c >= 32 && c <= 126) {
        resp += c;
      }
    } else if (resp.length() > 0) {
      break;
    }
  }
  
  if (resp.length() > 0) {
    outVal = resp;
    debugPrintf("[IR-DDS26D] Reg [%s] -> %s\n", obisCode, resp.c_str());
    if (g_dds26DumpIdx + resp.length() + 3 < sizeof(g_dds26RawDump)) {
      strcat(g_dds26RawDump, resp.c_str());
      strcat(g_dds26RawDump, "\r\n");
      g_dds26DumpIdx = strlen(g_dds26RawDump);
    }
    return true;
  }
  return false;
}

int MiddeDDS26DReader::readChar(uint32_t bitTimeUs, bool is7E1, unsigned long timeoutMs) {
  unsigned long start = millis();
  g_dds26PinState = (uint8_t)digitalRead(_rxPin);

  while (millis() - start < timeoutMs) {
    // Detección de Start Bit (flanco descendente HIGH -> LOW)
    if (digitalRead(_rxPin) == LOW) {
      g_dds26EdgeCount++;

      // Validar al 50% del ancho de bit
      delayMicroseconds(bitTimeUs / 2);
      if (digitalRead(_rxPin) != LOW) continue; // Ruido

      uint8_t val = 0;
      uint8_t bits = is7E1 ? 7 : 8;
      for (int i = 0; i < bits; i++) {
        delayMicroseconds(bitTimeUs);
        if (digitalRead(_rxPin) == HIGH) {
          val |= (1 << i);
        }
      }
      
      // Parity bit
      if (is7E1) {
        delayMicroseconds(bitTimeUs);
      }
      
      // Stop bit
      delayMicroseconds(bitTimeUs);

      // Esperar retorno a reposo
      unsigned long waitStop = micros();
      while (digitalRead(_rxPin) == LOW && (micros() - waitStop < bitTimeUs * 2)) {
      }

      uint8_t res = is7E1 ? (val & 0x7F) : val;
      g_dds26LastByte = res;
      return res;
    }
  }
  return -1;
}

bool MiddeDDS26DReader::readMeter(MeterData &data, unsigned long timeoutMs) {
  memset(&data, 0, sizeof(MeterData));
  data.lecturaValida = false;
  data.tipoMedidor = 1; // 1 = Monofásico (MIDDE DDS26D)
  data.estado = 0;

  memset(g_dds26RawDump, 0, sizeof(g_dds26RawDump));
  g_dds26DumpIdx = 0;
  memset(g_dds26Id, 0, sizeof(g_dds26Id));
  g_dds26LineCount = 0;
  strcpy(g_dds26ActiveMode, "Interrogating 2400 7E1");

  debugPrintln("\n========================================================");
  debugPrintln("[IR-DDS26D] Iniciando interrogación IEC 62056-21 a 2400 7E1");
  debugPrintln("========================================================");

  begin(2400);
  uint32_t bitTime2400 = 1000000UL / 2400;

  // Limpiar buffer RX
  while (readChar(bitTime2400, true, 30) >= 0);

  // 1. Envío de Sign-On /?!\r\n
  debugPrintln("[IR-DDS26D] 1. Enviando Sign-On (/?!\\r\\n) a 2400 baud 7E1...");
  digitalWrite(LED_2_PIN, LOW);
  sendString("/?!\r\n", bitTime2400, true);
  digitalWrite(LED_2_PIN, HIGH);

  // 2. Captura de la identificación (/STR3...)
  String idResponse = "";
  unsigned long startId = millis();
  while (millis() - startId < 2500) {
    int b = readChar(bitTime2400, true, idResponse.length() == 0 ? 500 : 150);
    if (b >= 0) {
      char c = (char)(b & 0x7F);
      idResponse += c;
      if (c == '\n') break;
    } else if (idResponse.length() > 0) {
      break;
    }
  }

  if (idResponse.length() == 0) {
    debugPrintln("[IR-DDS26D] Sin respuesta al Sign-on a 2400 baud.");
    strcpy(g_dds26ActiveMode, "No Sign-on Response");
    data.estado = 2;
    return false;
  }

  strncpy(g_dds26Id, idResponse.c_str(), sizeof(g_dds26Id) - 1);
  debugPrintf("[IR-DDS26D] ¡IDENTIFICACIÓN RECIBIDA!: %s\n", g_dds26Id);
  strcpy(g_dds26ActiveMode, "ID OK (2400 7E1)");

  // 3. Entrar a Modo 1 (Command Mode): ACK(0x06) + "031\r\n"
  delay(250); // tr = 250ms
  debugPrintln("[IR-DDS26D] 2. Enviando ACK Mode 1 (\\x06031\\r\\n)...");
  sendChar(0x06, bitTime2400, true);
  sendString("031\r\n", bitTime2400, true);
  delay(250);

  // 4. Lectura individual de registros OBIS
  debugPrintln("[IR-DDS26D] 3. Leyendo registros individuales OBIS:");

  String regVal = "";

  // A. Energía Activa Total Importada (1.8.0)
  if (readRegister("1.8.0()", regVal, bitTime2400)) {
    int p1 = regVal.indexOf('(');
    int p2 = regVal.indexOf('*', p1);
    if (p2 == -1) p2 = regVal.indexOf(')', p1);
    if (p1 != -1 && p2 != -1) {
      data.energiaActivaImp = (uint32_t)round(regVal.substring(p1 + 1, p2).toFloat() * 100.0f);
    }
  }

  // B. Tensión de Fase (32.7.0 ó 32.7)
  if (readRegister("32.7.0()", regVal, bitTime2400) || readRegister("32.7()", regVal, bitTime2400)) {
    int p1 = regVal.indexOf('(');
    int p2 = regVal.indexOf('*', p1);
    if (p2 == -1) p2 = regVal.indexOf(')', p1);
    if (p1 != -1 && p2 != -1) {
      data.voltajeA = (uint16_t)round(regVal.substring(p1 + 1, p2).toFloat());
    }
  }

  // C. Corriente de Fase (31.7.0 ó 31.7)
  if (readRegister("31.7.0()", regVal, bitTime2400) || readRegister("31.7()", regVal, bitTime2400)) {
    int p1 = regVal.indexOf('(');
    int p2 = regVal.indexOf('*', p1);
    if (p2 == -1) p2 = regVal.indexOf(')', p1);
    if (p1 != -1 && p2 != -1) {
      data.corrienteA = (uint16_t)round(regVal.substring(p1 + 1, p2).toFloat() * 100.0f);
    }
  }

  // D. Factor de Potencia (33.7.0 ó 33.7)
  if (readRegister("33.7.0()", regVal, bitTime2400) || readRegister("33.7()", regVal, bitTime2400)) {
    int p1 = regVal.indexOf('(');
    int p2 = regVal.indexOf('*', p1);
    if (p2 == -1) p2 = regVal.indexOf(')', p1);
    if (p1 != -1 && p2 != -1) {
      data.cosphi = (uint8_t)round(regVal.substring(p1 + 1, p2).toFloat() * 100.0f);
    }
  }

  // E. Frecuencia de Red (14.7.0 ó 14.7)
  if (readRegister("14.7.0()", regVal, bitTime2400) || readRegister("14.7()", regVal, bitTime2400)) {
    int p1 = regVal.indexOf('(');
    int p2 = regVal.indexOf('*', p1);
    if (p2 == -1) p2 = regVal.indexOf(')', p1);
    if (p1 != -1 && p2 != -1) {
      data.frecuenciaMin = (uint16_t)round(regVal.substring(p1 + 1, p2).toFloat() * 100.0f);
      data.frecuenciaMax = data.frecuenciaMin;
    }
  }

  // 5. Cierre de sesión (Break Command: <SOH> B 0 <ETX> <BCC>)
  delay(150);
  sendChar(0x01, bitTime2400, true);
  sendChar('B', bitTime2400, true);
  sendChar('0', bitTime2400, true);
  sendChar(0x03, bitTime2400, true);
  sendChar(0x71, bitTime2400, true); // 'B'^'0'^ETX = 0x42^0x30^0x03 = 0x71 ('q')

  // 7. Decodificación de registros OBIS capturados
  if (g_dds26DumpIdx > 0) {
    debugPrintf("\n[IR-DDS26D] Procesando trama capturada (%u caracteres)...\n", g_dds26DumpIdx);
    char dumpCopy[1024];
    strncpy(dumpCopy, g_dds26RawDump, sizeof(dumpCopy));
    char *linePtr = strtok(dumpCopy, "\r\n");
    while (linePtr != NULL) {
      String line = String(linePtr);
      line.trim();
      g_dds26LineCount++;

      // Tensión (1.0.32.7.0, 32.7.0, 32.7)
      if (line.startsWith("1.0.32.7.0") || line.startsWith("32.7.0") || line.indexOf("(V)") > 0 || line.startsWith("32.7(")) {
        int p1 = line.indexOf('(');
        int p2 = line.indexOf('*', p1);
        if (p2 == -1) p2 = line.indexOf(')', p1);
        if (p1 != -1 && p2 != -1) {
          data.voltajeA = (uint16_t)round(line.substring(p1 + 1, p2).toFloat());
        }
      }
      // Corriente (1.0.31.7.0, 31.7.0, 31.7)
      else if (line.startsWith("1.0.31.7.0") || line.startsWith("31.7.0") || line.indexOf("(A)") > 0 || line.startsWith("31.7(")) {
        int p1 = line.indexOf('(');
        int p2 = line.indexOf('*', p1);
        if (p2 == -1) p2 = line.indexOf(')', p1);
        if (p1 != -1 && p2 != -1) {
          data.corrienteA = (uint16_t)round(line.substring(p1 + 1, p2).toFloat() * 100.0f);
        }
      }
      // Energía Activa (1.0.1.8.0, 1.8.0)
      else if (line.startsWith("1.0.1.8.0") || line.startsWith("1.8.0") || line.indexOf("*kWh") > 0 || line.startsWith("1.8.0(")) {
        int p1 = line.indexOf('(');
        int p2 = line.indexOf('*', p1);
        if (p2 == -1) p2 = line.indexOf(')', p1);
        if (p1 != -1 && p2 != -1) {
          data.energiaActivaImp = (uint32_t)round(line.substring(p1 + 1, p2).toFloat() * 100.0f);
        }
      }
      // Factor de Potencia (1.0.33.7.0, 33.7.0)
      else if (line.startsWith("1.0.33.7.0") || line.startsWith("33.7.0") || line.startsWith("33.7(")) {
        int p1 = line.indexOf('(');
        int p2 = line.indexOf('*', p1);
        if (p2 == -1) p2 = line.indexOf(')', p1);
        if (p1 != -1 && p2 != -1) {
          data.cosphi = (uint8_t)round(line.substring(p1 + 1, p2).toFloat() * 100.0f);
        }
      }
      // Frecuencia (1.0.14.7.0, 14.7.0)
      else if (line.startsWith("1.0.14.7.0") || line.startsWith("14.7.0") || line.startsWith("14.7(")) {
        int p1 = line.indexOf('(');
        int p2 = line.indexOf('*', p1);
        if (p2 == -1) p2 = line.indexOf(')', p1);
        if (p1 != -1 && p2 != -1) {
          data.frecuenciaMin = (uint16_t)round(line.substring(p1 + 1, p2).toFloat() * 100.0f);
          data.frecuenciaMax = data.frecuenciaMin;
        }
      }

      linePtr = strtok(NULL, "\r\n");
    }
  }

  if (data.voltajeA > 50 || data.energiaActivaImp > 0 || g_dds26DumpIdx > 20) {
    data.lecturaValida = true;
    data.estado = 0;
    debugPrintln("\n[IR-DDS26D] ¡Lectura completada con éxito!");
    debugPrintf("[IR-DDS26D] VA=%u V | IA=%.2f A | EA=%.2f kWh | FP=%.2f | f=%.1f Hz\n",
                data.voltajeA, (float)data.corrienteA / 100.0f, (float)data.energiaActivaImp / 100.0f,
                (float)data.cosphi / 100.0f, (float)data.frecuenciaMin / 100.0f);
    return true;
  }

  data.lecturaValida = false;
  data.estado = 2; // Sin lectura completa
  return false;
}
