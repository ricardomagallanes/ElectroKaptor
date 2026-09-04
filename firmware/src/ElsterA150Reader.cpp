#include "ElsterA150Reader.h"
#include "MeterConfig.h"
#include "BoardConfig.h"
#include "DebugSerial.h"

// 2400 baudios -> 1 bit = 1000000 / 2400 = 416.67 us
#define BIT_TIME_US_2400 417
#define HALF_BIT_TIME_US 208

// Estructura de Diagnóstico en RAM para lectura SWD/OpenOCD
struct __attribute__((packed)) ElsterIRDiag {
  uint32_t magic;         // 0xEE150150
  uint32_t edgeCount;     // Contador de flancos en PA3
  uint16_t syncMatches;
  uint16_t bytesRead;
  uint8_t  lastRawByte;
  uint8_t  pinCurrentState;
  uint8_t  state;         // 0=idle, 1=syncing, 2=reading, 3=success, 4=timeout, 5=incomplete
  uint8_t  rawPacket[186];
};

volatile ElsterIRDiag g_elsterDiag = {0xEE150150, 0, 0, 0, 0, 0, 0, {0}};

ElsterA150Reader::ElsterA150Reader(uint8_t rxPin, uint8_t txPin) 
  : BaseMeterReader(rxPin, txPin) {}

void ElsterA150Reader::begin(unsigned long baudRate) {
  pinMode(_rxPin, INPUT_PULLUP);
  pinMode(_txPin, OUTPUT);
  digitalWrite(_txPin, HIGH);
}

int ElsterA150Reader::readByteFast(uint32_t timeoutUs) {
  uint32_t startWait = micros();
  g_elsterDiag.pinCurrentState = (uint8_t)digitalRead(_rxPin);

  // 1. Aguardar Start Bit (Línea pasa de HIGH a LOW)
  while (digitalRead(_rxPin) == HIGH) {
    if ((uint32_t)(micros() - startWait) > timeoutUs) {
      return -1; // Timeout
    }
  }

  g_elsterDiag.edgeCount++;
  notifyOpticalActivity();

  // 2. Start bit detectado: Deshabilitar interrupciones para muestreo atómico de alta precisión
  noInterrupts();

  // Muestrear al 50% del Start Bit para validar contra ruido
  delayMicroseconds(HALF_BIT_TIME_US);
  if (digitalRead(_rxPin) != LOW) {
    interrupts();
    return -2; // Glitch / falso inicio
  }

  // 3. Muestrear los 8 bits de datos secuenciales (LSB primero)
  uint8_t val = 0;
  for (int i = 0; i < 8; i++) {
    delayMicroseconds(BIT_TIME_US_2400);
    if (digitalRead(_rxPin) == HIGH) {
      val |= (1 << i);
    }
  }

  // 4. Muestrear el Stop Bit (HIGH)
  delayMicroseconds(BIT_TIME_US_2400);

  // Restaurar interrupciones inmediatamente
  interrupts();

  return (int)val;
}

uint8_t ElsterA150Reader::mapByteToNibble(uint8_t val) {
  switch (val) {
    case 0x55: return 0x0; // 85
    case 0x57: return 0x1; // 87
    case 0x5D: return 0x2; // 93
    case 0x5F: return 0x3; // 95
    case 0x75: return 0x4; // 117
    case 0x77: return 0x5; // 119
    case 0x7D: return 0x6; // 125
    case 0x7F: return 0x7; // 127
    case 0xD5: return 0x8; // 213
    case 0xD7: return 0x9; // 215
    case 0xDD: return 0xA; // 221
    case 0xDF: return 0xB; // 223
    case 0xF5: return 0xC; // 245
    case 0xF7: return 0xD; // 247
    case 0xFD: return 0xE; // 253
    case 0xFF: return 0xF; // 255
    default:   return 0x0;
  }
}

bool ElsterA150Reader::performOpticalRead(MeterData &data, unsigned long timeoutMs) {
  data.tipoMedidor = 1; // 1 = Monofasico (Elster A150)

  debugPrintln("\n[IR-ELSTER] Iniciando captura óptica Elster A150 (Monofasico)...");
  debugPrintf("[IR-ELSTER] Pines: RX=%d, TX=%d | Formato: 2400 baudios 8N1 (186 bytes)\n", _rxPin, _txPin);

  begin(2400);

  // Secuencia de sincronismo de 8 bytes oficial de Elster A150
  const uint8_t syncPattern[8] = {0x57, 0x55, 0x55, 0x55, 0x7F, 0x77, 0x5D, 0x55};
  const uint16_t numDatos = 186;
  uint8_t bytesRecibidos[numDatos];
  uint8_t bytesOrdenados[numDatos];
  uint8_t nibbles[numDatos];
  memset(bytesRecibidos, 0, sizeof(bytesRecibidos));

  g_elsterDiag.magic = 0xEE150150;
  g_elsterDiag.syncMatches = 0;
  g_elsterDiag.bytesRead = 0;
  g_elsterDiag.state = 1; // Syncing

  // Limpiar cualquier byte residual en la línea
  while (readByteFast(2000) >= 0);

  debugPrintln("[IR-ELSTER] Aguardando patrón de sincronismo (57 55 55 55 7F 77 5D 55)...");

  uint8_t syncIdx = 0;
  unsigned long startWait = millis();
  bool syncFound = false;

  while (millis() - startWait < timeoutMs) {
    notifyOpticalActivity();

    // Esperar hasta 50ms por un byte entrante
    int b = readByteFast(50000);
    if (b < 0) {
      continue;
    }

    uint8_t val = (uint8_t)b;
    g_elsterDiag.lastRawByte = val;

    if (val == syncPattern[syncIdx]) {
      bytesRecibidos[syncIdx] = val;
      syncIdx++;
      g_elsterDiag.syncMatches = syncIdx;
      if (syncIdx == 8) {
        syncFound = true;
        break; // ¡Cabecera completa de 8 bytes detectada!
      }
    } else {
      if (val == syncPattern[0]) {
        bytesRecibidos[0] = val;
        syncIdx = 1;
      } else {
        syncIdx = 0;
      }
      g_elsterDiag.syncMatches = syncIdx;
    }
  }

  if (!syncFound) {
    debugPrintln("[IR-ELSTER] Timeout esperando cabecera de sincronismo Elster A150.");
    g_elsterDiag.state = 4; // Timeout
    return false;
  }

  debugPrintln("[IR-ELSTER] ¡Sincronismo detectado! Leyendo paquete de 186 bytes...");
  g_elsterDiag.state = 2; // Reading

  // 2. Leer los bytes restantes del frame (hasta 186 total)
  uint16_t totalBytes = 8;
  for (uint16_t i = 8; i < numDatos; i++) {
    notifyOpticalActivity();
    int b = readByteFast(25000); // 25ms timeout inter-byte
    if (b < 0) {
      if (i >= 180) {
        // Paquete completo: el medidor finalizó su transmisión en 180..185 bytes (contiene toda la telemetría)
        debugPrintf("[IR-ELSTER] Paquete completado en byte %d (>=180).\n", i);
        totalBytes = i;
        break;
      }
      debugPrintf("[IR-ELSTER] Error: Paquete incompleto en byte %d/186\n", i);
      g_elsterDiag.state = 5; // Incomplete
      return false;
    }
    bytesRecibidos[i] = (uint8_t)b;
    totalBytes = i + 1;
    g_elsterDiag.bytesRead = totalBytes;
  }

  memcpy((void*)g_elsterDiag.rawPacket, bytesRecibidos, totalBytes < 186 ? totalBytes : 186);

  g_elsterDiag.state = 3; // Success
  debugPrintf("[IR-ELSTER] Paquete de %d bytes recibido. Decodificando nibbles...\n", totalBytes);

  // 3. Swap de bytes adyacentes (según especificación oficial y código probado medicion_que_anda.ino)
  for (uint16_t i = 8; i < totalBytes; i += 2) {
    if (i + 1 < totalBytes) {
      bytesOrdenados[i]     = bytesRecibidos[i + 1];
      bytesOrdenados[i + 1] = bytesRecibidos[i];
    } else {
      bytesOrdenados[i]     = bytesRecibidos[i];
    }
  }

  // 4. Mapear cada byte a su valor nibble real (4 bits)
  for (uint16_t i = 8; i < totalBytes; i++) {
    nibbles[i] = mapByteToNibble(bytesOrdenados[i]);
  }

  // 5. Decodificar magnitudes eléctricas según el mapa de memoria oficial de Elster A150
  // Voltaje Fase A (índices 128 a 130, 3 dígitos BCD: ej. 220V / 230V)
  uint16_t valV = 0;
  if (totalBytes > 130 && nibbles[128] <= 9 && nibbles[129] <= 9 && nibbles[130] <= 9) {
    valV = (nibbles[128] * 100) + (nibbles[129] * 10) + nibbles[130];
  }
  // Tolerancia alternativa en caso de desplazamiento de 1 nibble en variantes del firmware del medidor
  if ((valV < 80 || valV > 300) && totalBytes > 131 && nibbles[129] <= 9 && nibbles[130] <= 9 && nibbles[131] <= 9) {
    uint16_t altV = (nibbles[129] * 100) + (nibbles[130] * 10) + nibbles[131];
    if (altV >= 80 && altV <= 300) {
      valV = altV;
    }
  }
  data.voltajeA = valV;
  data.voltajeB = 0;
  data.voltajeC = 0;

  // Corriente Fase A (índices 123 a 125, 3 dígitos BCD desplazados >> 2)
  uint32_t valI = 0;
  if (totalBytes > 125 && nibbles[123] <= 9 && nibbles[124] <= 9 && nibbles[125] <= 9) {
    valI = (nibbles[123] * 100) + (nibbles[124] * 10) + nibbles[125];
    valI = valI >> 2;
  }
  data.corrienteA = (uint16_t)valI; // Centésimas de Ampere (0.01 A)
  data.corrienteB = 0;
  data.corrienteC = 0;

  // Factor de Potencia Cosφ (índices 115 a 116, 2 dígitos BCD)
  uint32_t valCos = 0;
  if (totalBytes > 116 && nibbles[115] <= 9 && nibbles[116] <= 9) {
    valCos = (nibbles[115] * 10) + nibbles[116];
    if (valCos > 100) valCos = 0;
  }
  data.cosphi = (uint8_t)valCos;

  // Energía Activa Importada (índices 62 a 69, 8 dígitos BCD)
  uint32_t valE = 0;
  if (totalBytes > 69) {
    for (int i = 62; i <= 69; i++) {
      if (nibbles[i] <= 9) {
        valE = (valE * 10) + nibbles[i];
      }
    }
  }
  data.energiaActivaImp = valE; // En centésimas de kWh (0.01 kWh)

  // Energía Reactiva Importada (índices 77 a 80, 4 dígitos BCD)
  uint32_t valR = 0;
  if (totalBytes > 80) {
    for (int i = 77; i <= 80; i++) {
      if (nibbles[i] <= 9) {
        valR = (valR * 10) + nibbles[i];
      }
    }
  }
  data.energiaReactivaImp = valR;

  // Demanda Máxima Activa (índices 177 a 179)
  if (totalBytes >= 180) {
    uint32_t valD = 0;
    for (int i = 177; i <= 179; i++) {
      if (nibbles[i] <= 9) {
        valD = (valD * 10) + nibbles[i];
      }
    }
    data.maximaDemandaImp = valD;
  }

  if (data.voltajeA > 0 || data.energiaActivaImp > 0) {
    data.lecturaValida = true;
    data.estado = 0;
  }



  debugPrintln("\n==================================================");
  debugPrintln("=== ¡¡¡PARÁMETROS DECODIFICADOS ELSTER A150!!! ===");
  debugPrintln("==================================================");
  debugPrintf(" • Tipo de Medidor          : Monofásico (Tipo 1)\n");
  debugPrintf(" • Voltaje Fase A           : %u V\n", data.voltajeA);
  debugPrintf(" • Corriente Fase A         : %.2f A\n", data.corrienteA / 100.0f);
  debugPrintf(" • Factor de Potencia (Cosφ): %.2f\n", data.cosphi / 100.0f);
  debugPrintf(" • Energía Activa Importada : %.2f kWh\n", data.energiaActivaImp / 100.0f);
  debugPrintf(" • Energía Reactiva Import. : %.2f kVARh\n", data.energiaReactivaImp / 100.0f);
  debugPrintf(" • Demanda Máxima Activa    : %.2f kW\n", data.maximaDemandaImp / 100.0f);
  debugPrintln("==================================================");

  return true;
}
