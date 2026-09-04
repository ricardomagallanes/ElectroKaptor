#include "HexingHXE34KReader.h"
#include "MeterConfig.h"
#include "BoardConfig.h"
#include "DebugSerial.h"

// 300 baudios: 1/300 = 3333.33 us por bit
#define BIT_TIME_US_300 3333
// 2400 baudios: 1/2400 = 416.67 us por bit
#define BIT_TIME_US_2400 417

// Estructura de Diagnóstico en RAM para inspección con OpenOCD / SWD
struct __attribute__((packed)) HexingDiag {
  uint32_t magic;         // 0xEE340000
  uint16_t lineCount;
  uint16_t bytesRead;
  uint16_t lastVoltageA;
  uint16_t lastVoltageB;
  uint16_t lastVoltageC;
  uint32_t lastEnergyImp;
  uint8_t  state;         // 1=wake/sign-on, 2=receiving, 4=success, 5=error
  uint8_t  padding[3];
  char     meterId[64];
  char     rawDump[1024];
};

volatile HexingDiag g_hexingDiag = {0xEE340000, 0, 0, 0, 0, 0, 0, 0, {0}, {0}, {0}};

HexingHXE34KReader::HexingHXE34KReader(uint8_t rxPin, uint8_t txPin) 
  : BaseMeterReader(rxPin, txPin) {}

void HexingHXE34KReader::begin(unsigned long baudRate) {
  pinMode(_txPin, OUTPUT);
  digitalWrite(_txPin, HIGH); // IDLE en HIGH para infrarrojo
  pinMode(_rxPin, INPUT_PULLUP);
}

void HexingHXE34KReader::sendWakeUpOptical(uint16_t durationMs) {
  // Secuencia de wake-up para activar el microcontrolador óptico del Hexing HXE34K
  // Transmite una ráfaga de ceros/modulación en TX para despertar el puerto óptico
  debugPrintln("[IR-HEXING] Enviando secuencia de wake-up óptico...");
  unsigned long start = millis();
  while (millis() - start < durationMs) {
    digitalWrite(_txPin, LOW);
    delayMicroseconds(500);
    digitalWrite(_txPin, HIGH);
    delayMicroseconds(500);
  }
  digitalWrite(_txPin, HIGH);
  delay(200); // Pausa de estabilización requerida por IEC 62056-21
}

void HexingHXE34KReader::sendCharBitbang(char c, uint32_t bitTimeUs) {
  uint8_t val = (uint8_t)c & 0x7F;
  uint8_t parity = 0;
  for (int i = 0; i < 7; i++) {
    if (val & (1 << i)) parity ^= 1;
  }

  // Start bit: LOW
  digitalWrite(_txPin, LOW);
  delayMicroseconds(bitTimeUs);

  // 7 Bits de datos (LSB primero)
  for (int i = 0; i < 7; i++) {
    digitalWrite(_txPin, (val & (1 << i)) ? HIGH : LOW);
    delayMicroseconds(bitTimeUs);
  }

  // Bit de Paridad Par (7E1)
  digitalWrite(_txPin, parity ? HIGH : LOW);
  delayMicroseconds(bitTimeUs);

  // Stop bit: HIGH
  digitalWrite(_txPin, HIGH);
  delayMicroseconds(bitTimeUs * 2); // 2 stop bits para compatibilidad robusta
}

void HexingHXE34KReader::sendStringBitbang(const char* str, uint32_t bitTimeUs) {
  while (*str) {
    sendCharBitbang(*str++, bitTimeUs);
  }
}

int HexingHXE34KReader::readCharBitbang(unsigned long timeoutMs, uint32_t bitTimeUs) {
  unsigned long start = millis();

  while (millis() - start < timeoutMs) {
    // Detección de Start Bit (flanco descendente HIGH -> LOW)
    if (digitalRead(_rxPin) == LOW) {
      notifyOpticalActivity();

      // Muestrear en el 50% del Start Bit para validar señal
      delayMicroseconds(bitTimeUs / 2);
      if (digitalRead(_rxPin) != LOW) continue; // Ruido / falso disparo

      uint8_t val = 0;
      for (int i = 0; i < 7; i++) {
        delayMicroseconds(bitTimeUs);
        if (digitalRead(_rxPin) == HIGH) {
          val |= (1 << i);
        }
      }

      // Parity bit y Stop bit
      delayMicroseconds(bitTimeUs);
      delayMicroseconds(bitTimeUs);

      // Esperar a que la línea retorne a reposo (HIGH)
      unsigned long waitStop = micros();
      while (digitalRead(_rxPin) == LOW && (micros() - waitStop < bitTimeUs * 2)) {
      }

      return val & 0x7F;
    }
  }
  return -1;
}

int HexingHXE34KReader::readByteFast2400(uint32_t timeoutUs) {
  uint32_t startWait = micros();

  while (digitalRead(_rxPin) == HIGH) {
    if ((uint32_t)(micros() - startWait) > timeoutUs) {
      return -1;
    }
  }

  uint32_t t0 = micros();
  uint8_t rawVal = 0;

  for (int i = 0; i < 8; i++) {
    uint32_t slotStart = t0 + ((i + 1) * BIT_TIME_US_2400) + 40;
    uint32_t slotEnd   = t0 + ((i + 2) * BIT_TIME_US_2400) - 20;

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

  return (int)((~rawVal) & 0x7F);
}

static float extractObisValue(const String &line) {
  int p1 = line.indexOf('(');
  if (p1 == -1) return 0.0f;
  int p2 = line.indexOf('*', p1);
  if (p2 == -1) p2 = line.indexOf(')', p1);
  if (p2 == -1) return 0.0f;
  return line.substring(p1 + 1, p2).toFloat();
}

static void parseHexingObisLine(const String &line, MeterData &data) {
  float val = extractObisValue(line);

  // 1. Tensiones por Fase (OBIS estándar Hexing HXE34K: 32.7.0, 52.7.0, 72.7.0 / 32.5.0, etc.)
  if (line.indexOf("32.7.0") >= 0 || line.indexOf("1.0.32.7") >= 0 || line.indexOf("32.5.0") >= 0 || line.indexOf("32.7(") >= 0) {
    data.voltajeA = (uint16_t)round(val);
  } else if (line.indexOf("52.7.0") >= 0 || line.indexOf("1.0.52.7") >= 0 || line.indexOf("52.5.0") >= 0 || line.indexOf("52.7(") >= 0) {
    data.voltajeB = (uint16_t)round(val);
  } else if (line.indexOf("72.7.0") >= 0 || line.indexOf("1.0.72.7") >= 0 || line.indexOf("72.5.0") >= 0 || line.indexOf("72.7(") >= 0) {
    data.voltajeC = (uint16_t)round(val);
  }

  // 2. Corrientes por Fase (OBIS estándar Hexing HXE34K: 31.7.0, 51.7.0, 71.7.0 / 31.5.0, etc.)
  else if (line.indexOf("31.7.0") >= 0 || line.indexOf("1.0.31.7") >= 0 || line.indexOf("31.5.0") >= 0 || line.indexOf("31.7(") >= 0) {
    data.corrienteA = (uint16_t)round(val * 100.0f);
  } else if (line.indexOf("51.7.0") >= 0 || line.indexOf("1.0.51.7") >= 0 || line.indexOf("51.5.0") >= 0 || line.indexOf("51.7(") >= 0) {
    data.corrienteB = (uint16_t)round(val * 100.0f);
  } else if (line.indexOf("71.7.0") >= 0 || line.indexOf("1.0.71.7") >= 0 || line.indexOf("71.5.0") >= 0 || line.indexOf("71.7(") >= 0) {
    data.corrienteC = (uint16_t)round(val * 100.0f);
  }

  // 3. Energía Activa Importada Total (1.8.0 / 1.0.1.8.0 / 1.8.0.1)
  else if (line.indexOf("1.8.0") >= 0 || line.indexOf("1.0.1.8.0") >= 0 || line.indexOf("1.8.1") >= 0) {
    if (val > 0.0f && data.energiaActivaImp == 0) {
      data.energiaActivaImp = (uint32_t)round(val * 100.0f);
      g_hexingDiag.lastEnergyImp = data.energiaActivaImp;
    }
  }

  // 4. Energía Activa Exportada Total (2.8.0 / 1.0.2.8.0)
  else if (line.indexOf("2.8.0") >= 0 || line.indexOf("1.0.2.8.0") >= 0) {
    data.energiaActivaExp = (uint32_t)round(val * 100.0f);
  }

  // 5. Energía Reactiva Importada Total (3.8.0 / 1.0.3.8.0)
  else if (line.indexOf("3.8.0") >= 0 || line.indexOf("1.0.3.8.0") >= 0) {
    data.energiaReactivaImp = (uint32_t)round(val * 100.0f);
  }

  // 6. Energía Reactiva Exportada Total (4.8.0 / 1.0.4.8.0)
  else if (line.indexOf("4.8.0") >= 0 || line.indexOf("1.0.4.8.0") >= 0) {
    data.energiaReactivaExp = (uint32_t)round(val * 100.0f);
  }

  // 7. Máxima Demanda de Importación (1.6.0 / 1.2.0 / 1.5.0)
  else if (line.indexOf("1.6.0") >= 0 || line.indexOf("1.2.0") >= 0 || line.indexOf("1.5.0") >= 0) {
    if (val > 0.0f && data.maximaDemandaImp == 0) {
      data.maximaDemandaImp = (uint32_t)round(val * 100.0f);
    }
  }

  // 8. Factor de Potencia Instantáneo Cos φ (13.7.0 / 13.5.0 / 33.7.0)
  else if (line.indexOf("13.7.0") >= 0 || line.indexOf("13.5.0") >= 0 || line.indexOf("33.7.0") >= 0) {
    data.cosphi = (uint8_t)round(val * 100.0f);
  }

  // 9. Frecuencia de Red (14.7.0 / 14.5.0)
  else if (line.indexOf("14.7.0") >= 0 || line.indexOf("14.5.0") >= 0) {
    data.frecuenciaMin = (uint16_t)round(val * 100.0f);
    data.frecuenciaMax = data.frecuenciaMin;
  }
}

bool HexingHXE34KReader::performOpticalRead(MeterData &data, unsigned long timeoutMs) {
  // Inicialización estricta en 0 (Directiva Mandatoria AGENTS.md / README.md)
  data.tipoMedidor = 2; // Trifásico (Hexing HXE34K)
  data.voltajeA = 0;
  data.voltajeB = 0;
  data.voltajeC = 0;
  data.corrienteA = 0;
  data.corrienteB = 0;
  data.corrienteC = 0;
  data.cosphi = 0;
  data.energiaActivaImp = 0;
  data.energiaActivaExp = 0;
  data.energiaReactivaImp = 0;
  data.energiaReactivaExp = 0;
  data.maximaDemandaImp = 0;
  data.maximaDemandaExp = 0;
  data.frecuenciaMin = 0;
  data.frecuenciaMax = 0;
  data.lecturaValida = false;

  debugPrintln("\n[IR-HEXING] Iniciando interrogación óptica Hexing Trifásico HXE34K...");
  debugPrintf("[IR-HEXING] Pines: RX=%d, TX=%d | Protocolo: IEC 62056-21 Modo C (300 baudios 7E1)...\n", _rxPin, _txPin);

  begin(300);

  g_hexingDiag.magic = 0xEE340000;
  g_hexingDiag.lineCount = 0;
  g_hexingDiag.bytesRead = 0;
  g_hexingDiag.lastVoltageA = 0;
  g_hexingDiag.lastVoltageB = 0;
  g_hexingDiag.lastVoltageC = 0;
  g_hexingDiag.lastEnergyImp = 0;
  g_hexingDiag.state = 1; // Wake / Sign-on
  memset((char*)g_hexingDiag.meterId, 0, sizeof(g_hexingDiag.meterId));
  memset((char*)g_hexingDiag.rawDump, 0, sizeof(g_hexingDiag.rawDump));

  // 1. Secuencia de Wake-up óptico para sacar al medidor de reposo de bajo consumo
  sendWakeUpOptical(250);

  // Limpiar posibles ecos o residuos en la línea
  while (readCharBitbang(50, BIT_TIME_US_300) >= 0);

  // 2. Envío del comando Sign-on estándar IEC 62056-21 (/?!\r\n)
  debugPrintln("[IR-HEXING] Enviando comando Sign-on (/?!\\r\\n)...");
  sendStringBitbang("/?!\r\n", BIT_TIME_US_300);

  // 3. Captura de la respuesta de identificación del medidor (/HEX5... o /HXT...)
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

  // Si no respondió a Sign-on, verificar si el medidor está en emisión espontánea
  if (idResponse.length() == 0) {
    debugPrintln("[IR-HEXING] Sin respuesta inmediata a Sign-on 300 baud. Verificando emisión espontánea a 2400 baud...");
    int dumpIdx = 0;
    unsigned long burstWait = millis() + 3500;
    while (millis() < burstWait && dumpIdx < (int)sizeof(g_hexingDiag.rawDump) - 2) {
      int b = readByteFast2400(100000);
      if (b >= 0) {
        g_hexingDiag.rawDump[dumpIdx++] = (char)b;
        g_hexingDiag.rawDump[dumpIdx] = '\0';
      } else if (dumpIdx > 50) {
        break;
      }
    }

    if (dumpIdx > 50) {
      debugPrintf("[IR-HEXING] ¡Ráfaga espontánea detectada! (%d bytes)\n", dumpIdx);
      g_hexingDiag.bytesRead = dumpIdx;
      // Procesar líneas OBIS desde la ráfaga
      char copyBuf[1024];
      strncpy(copyBuf, (char*)g_hexingDiag.rawDump, sizeof(copyBuf));
      char *linePtr = strtok(copyBuf, "\r\n");
      int lCount = 0;
      while (linePtr != NULL) {
        String l = String(linePtr);
        l.trim();
        if (l.length() > 0) {
          lCount++;
          parseHexingObisLine(l, data);
        }
        linePtr = strtok(NULL, "\r\n");
      }
      g_hexingDiag.lineCount = lCount;
      if (lCount >= 3 || data.voltajeA > 0 || data.voltajeB > 0 || data.voltajeC > 0 || data.energiaActivaImp > 0) {
        g_hexingDiag.state = 4;
        data.lecturaValida = true;
        return true;
      }
    }

    debugPrintln("[IR-HEXING] Error: El medidor Hexing no respondió en el puerto óptico.");
    g_hexingDiag.state = 5;
    return false;
  }

  idResponse.trim();
  strncpy((char*)g_hexingDiag.meterId, idResponse.c_str(), sizeof(g_hexingDiag.meterId) - 1);
  debugPrintf("[IR-HEXING] Identificación recibida: %s\n", idResponse.c_str());

  // 4. Enviar confirmación ACK Modo C (Readout a 300 baudios): ACK(0x06) + "000\r\n"
  delay(150);
  debugPrintln("[IR-HEXING] Enviando ACK (\\x06000\\r\\n) para solicitar registros OBIS...");
  sendCharBitbang(0x06, BIT_TIME_US_300);
  sendStringBitbang("000\r\n", BIT_TIME_US_300);

  // 5. Captura y decodificación en streaming del bloque de registros OBIS
  debugPrintln("[IR-HEXING] Recibiendo y procesando registros OBIS Hexing HXE34K:");
  g_hexingDiag.state = 2; // Receiving
  unsigned long obisTimeout = millis() + 15000;
  String obisBuffer = "";
  int lineCount = 0;
  int dumpIdx = 0;

  while (millis() < obisTimeout) {
    notifyOpticalActivity();
    int b = readCharBitbang(450, BIT_TIME_US_300);
    if (b >= 0) {
      char c = (char)(b & 0x7F);
      debugPrintChar(c);
      obisBuffer += c;
      obisTimeout = millis() + 3500; // Refrescar timeout dinámico mientras haya flujo de caracteres

      if (dumpIdx < (int)sizeof(g_hexingDiag.rawDump) - 2) {
        g_hexingDiag.rawDump[dumpIdx++] = c;
        g_hexingDiag.rawDump[dumpIdx] = '\0';
      }

      if (c == '\n') {
        String line = obisBuffer;
        line.trim();
        lineCount++;
        g_hexingDiag.lineCount = lineCount;

        parseHexingObisLine(line, data);

        obisBuffer = "";
      }

      // Fin de bloque estándar IEC 62056-21 (! o ETX 0x03)
      if (c == '!' || c == 0x03) {
        debugPrintln("\n[IR-HEXING] Fin de bloque detectado (! / ETX).");
        break;
      }
    }
  }

  g_hexingDiag.bytesRead = dumpIdx;
  debugPrintf("\n[IR-HEXING] Total de líneas procesadas: %d | Bytes: %d\n", lineCount, dumpIdx);

  // 6. Validación de lectura: Se acepta si hay registros válidos o energía real
  if (lineCount >= 3 || data.voltajeA > 0 || data.voltajeB > 0 || data.voltajeC > 0 || data.energiaActivaImp > 0) {
    g_hexingDiag.state = 4; // Success
    data.lecturaValida = true;

    g_hexingDiag.lastVoltageA = data.voltajeA;
    g_hexingDiag.lastVoltageB = data.voltajeB;
    g_hexingDiag.lastVoltageC = data.voltajeC;

    debugPrintln("\n==================================================");
    debugPrintln("=== ¡¡¡PARÁMETROS DECODIFICADOS HEXING HXE34K!!! ===");
    debugPrintln("==================================================");
    debugPrintf(" • Tipo de Medidor          : Trifásico (Tipo 2)\n");
    debugPrintf(" • Identificador / Modelo   : %s\n", g_hexingDiag.meterId);
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

  g_hexingDiag.state = 5; // Error
  return false;
}
