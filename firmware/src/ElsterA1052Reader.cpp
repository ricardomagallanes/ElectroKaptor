#include "ElsterA1052Reader.h"
#include "MeterConfig.h"
#include "BoardConfig.h"
#include "DebugSerial.h"

// 2400 baudios -> 1 bit = 1000000 / 2400 = 416.67 us
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
  char     rawDump[1536]; // Buffer ampliado para capturar la totalidad de los 55 registros OBIS
};

volatile ElsterA1052Diag g_a1052Diag = {0xEE105200, 0, 0, 0, 0, 0, 0, 0, {0}, {0}};

ElsterA1052Reader::ElsterA1052Reader(uint8_t rxPin, uint8_t txPin) 
  : BaseMeterReader(rxPin, txPin) {}

void ElsterA1052Reader::begin(unsigned long baudRate) {
  pinMode(_txPin, OUTPUT);
  digitalWrite(_txPin, HIGH);
  pinMode(_rxPin, INPUT_PULLUP);
}

static float extractObisValue(const char* line) {
  const char* pOpen = strchr(line, '(');
  if (!pOpen) return 0.0f;
  const char* pEnd = strchr(pOpen, '*');
  if (!pEnd) pEnd = strchr(pOpen, ')');
  if (!pEnd || pEnd <= (pOpen + 1)) return 0.0f;

  char buf[32];
  size_t len = pEnd - (pOpen + 1);
  if (len >= sizeof(buf)) len = sizeof(buf) - 1;
  memcpy(buf, pOpen + 1, len);
  buf[len] = '\0';
  return atof(buf);
}

// Muestreo por ventana continua rápida de pulso a 2400 baudios (captura pulsos ópticos reales)
int ElsterA1052Reader::readByteFast2400(uint32_t timeoutUs) {
  uint32_t startWait = micros();

  // 1. Aguardar flanco descendente del Start bit (Línea pasa de HIGH a LOW por pulso IR)
  while (digitalRead(_rxPin) == HIGH) {
    if ((uint32_t)(micros() - startWait) > timeoutUs) {
      return -1; // Timeout
    }
  }

  uint32_t t0 = micros();
  uint8_t rawVal = 0;

  // 2. Muestrear los 8 slots de bits a 2400 baudios (417 us por slot, saltando el Start Bit)
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

  // Inversión lógica de modulación IR (pulso LOW = bit 0) y máscara 7-bit (elimina bit de paridad 7E1)
  uint8_t val = ((~rawVal) & 0x7F);
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

static void parseObisLine(const char* line, MeterData &data) {
  float val = extractObisValue(line);

  // 1. Tensiones por Fase (OBIS oficial Elster A1052 según Resumen Dispositivos con Obis.pdf)
  // Item 46: 32.5.0(231.3*V) -> Tensión Fase A
  // Item 47: 52.5.0(000.0*V) -> Tensión Fase B
  // Item 48: 72.5.0(000.0*V) -> Tensión Fase C
  if (strstr(line, "32.5.0") || strstr(line, "32.7.0") || strstr(line, "32.5(")) {
    data.voltajeA = (uint16_t)round(val);
  } else if (strstr(line, "52.5.0") || strstr(line, "52.7.0") || strstr(line, "52.5(")) {
    data.voltajeB = (uint16_t)round(val);
  } else if (strstr(line, "72.5.0") || strstr(line, "72.7.0") || strstr(line, "72.5(")) {
    data.voltajeC = (uint16_t)round(val);
  }

  // 2. Corrientes por Fase (OBIS oficial Elster A1052)
  // Item 49: 31.5.0(002.3*A) -> Corriente Fase A
  // Item 50: 51.5.0(000.0*A) -> Corriente Fase B
  // Item 51: 71.5.0(000.0*A) -> Corriente Fase C
  else if (strstr(line, "31.5.0") || strstr(line, "31.7.0") || strstr(line, "31.5(")) {
    data.corrienteA = (uint16_t)round(val * 100.0f);
  } else if (strstr(line, "51.5.0") || strstr(line, "51.7.0") || strstr(line, "51.5(")) {
    data.corrienteB = (uint16_t)round(val * 100.0f);
  } else if (strstr(line, "71.5.0") || strstr(line, "71.7.0") || strstr(line, "71.5(")) {
    data.corrienteC = (uint16_t)round(val * 100.0f);
  }

  // 3. Energía Activa Importada Total
  // Item 5: 1.8.0(003321.011*kWh)
  else if (strstr(line, "1.8.0(") || strstr(line, "1.8.0*")) {
    if (val > 0.0f) {
      data.energiaActivaImp = (uint32_t)round(val * 100.0f);
      g_a1052Diag.lastEnergyImp = data.energiaActivaImp;
    }
  }

  // 4. Energía Activa Exportada Total
  // Item 6: 2.8.0(000000.000*kWh)
  else if (strstr(line, "2.8.0(") || strstr(line, "2.8.0*")) {
    data.energiaActivaExp = (uint32_t)round(val * 100.0f);
  }

  // 5. Energía Reactiva Importada Total
  // Item 7: 3.8.0(000720.658*kVArh)
  else if (strstr(line, "3.8.0(") || strstr(line, "3.8.0*")) {
    data.energiaReactivaImp = (uint32_t)round(val * 100.0f);
  }

  // 6. Energía Reactiva Exportada Total
  // Item 8: 4.8.0(000004.054*kVArh)
  else if (strstr(line, "4.8.0(") || strstr(line, "4.8.0*")) {
    data.energiaReactivaExp = (uint32_t)round(val * 100.0f);
  }

  // 7. Máxima Demanda de Importación
  // Item 17: 1.6.0(00000.944*kW) o Item 21: 1.2.0(...) o Item 13: 1.5.0(...)
  else if (strstr(line, "1.6.0(") || strstr(line, "1.5.0(") || strstr(line, "1.2.0(")) {
    if (val > 0.0f && data.maximaDemandaImp == 0) {
      data.maximaDemandaImp = (uint32_t)round(val * 100.0f);
    }
  }

  // 8. Factor de Potencia Instantáneo Cos φ
  else if (strstr(line, "13.5.0") || strstr(line, "13.7.0") || strstr(line, "33.7.0")) {
    data.cosphi = (uint8_t)round(val * 100.0f);
  }

  // 9. Frecuencia de Red
  else if (strstr(line, "14.5.0") || strstr(line, "14.7.0")) {
    data.frecuenciaMin = (uint16_t)round(val * 100.0f);
    data.frecuenciaMax = data.frecuenciaMin;
  }
}

bool ElsterA1052Reader::performOpticalRead(MeterData &data, unsigned long timeoutMs) {
  // Inicialización estricta en 0 (Directiva Mandatoria AGENTS.md / README.md)
  data.tipoMedidor = 2; // Trifásico (Elster A1052)
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

  // Limpiar residuos en buffer
  while (readByteFast2400(2000) >= 0);

  unsigned long hardDeadline = millis() + timeoutMs;
  uint16_t dumpIdx = 0;

  // Capturar bytes consecutivamente sin bloqueos ni parseos intermedios
  while (millis() < hardDeadline && dumpIdx < sizeof(g_a1052Diag.rawDump) - 2) {
    if ((dumpIdx & 0x1F) == 0) {
      notifyOpticalActivity();
    }

    // Si aún no inició la ráfaga, esperar hasta 3.5s; una vez iniciada, timeout de 100ms indica fin de ráfaga
    uint32_t waitTimeoutUs = (dumpIdx == 0) ? 3500000 : 100000;
    int b = readByteFast2400(waitTimeoutUs);

    if (b >= 0) {
      g_a1052Diag.rawDump[dumpIdx++] = (char)b;
      g_a1052Diag.rawDump[dumpIdx] = '\0';
    } else {
      if (dumpIdx > 50) {
        // Fin de ráfaga tras recibir contenido
        debugPrintln("[IR-A1052] Fin de ráfaga detectado por silencio.");
        break;
      }
    }
  }

  g_a1052Diag.bytesRead = dumpIdx;
  debugPrintf("[IR-A1052] Total de bytes capturados: %u bytes\n", dumpIdx);

  if (dumpIdx < 10) {
    debugPrintln("[IR-A1052] Error: No se recibieron datos suficientes en la ráfaga.");
    g_a1052Diag.state = 5; // Error
    return false;
  }

  // Decodificación línea a línea de los registros OBIS almacenados en memoria
  char lineBuf[128];
  uint16_t lineBufIdx = 0;
  uint16_t totalLines = 0;

  for (uint16_t i = 0; i <= dumpIdx; i++) {
    char c = (i < dumpIdx) ? g_a1052Diag.rawDump[i] : '\n';
    if (c == '\r') continue;
    if (c == '\n') {
      lineBuf[lineBufIdx] = '\0';
      if (lineBufIdx > 0) {
        totalLines++;
        debugPrintf("[IR-A1052-OBIS] %s\n", lineBuf);
        parseObisLine(lineBuf, data);
      }
      lineBufIdx = 0;
    } else if (lineBufIdx < sizeof(lineBuf) - 1) {
      lineBuf[lineBufIdx++] = c;
    }
  }

  g_a1052Diag.lineCount = totalLines;
  debugPrintf("[IR-A1052] Total de líneas OBIS procesadas: %u\n", totalLines);

  // Verificación de éxito de lectura: Se acepta si se recibieron registros válidos o energía real
  if (totalLines >= 3 || data.voltajeA > 0 || data.voltajeB > 0 || data.voltajeC > 0 || data.energiaActivaImp > 0) {
    g_a1052Diag.state = 4; // Success
    data.lecturaValida = true;

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
