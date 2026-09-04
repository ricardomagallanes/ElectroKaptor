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
  uint8_t  sampleBuffer[16];
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

  // 2. Leer los 178 bytes restantes del frame (186 total - 8 sincronismo)
  for (uint16_t i = 8; i < numDatos; i++) {
    notifyOpticalActivity();
    int b = readByteFast(10000); // 10ms timeout inter-byte
    if (b < 0) {
      debugPrintf("[IR-ELSTER] Error: Paquete incompleto en byte %d/186\n", i);
      g_elsterDiag.state = 5; // Incomplete
      return false;
    }
    bytesRecibidos[i] = (uint8_t)b;
    g_elsterDiag.bytesRead = i + 1;
    if (i < 24) {
      g_elsterDiag.sampleBuffer[i - 8] = (uint8_t)b;
    }
  }

  g_elsterDiag.state = 3; // Success
  debugPrintln("[IR-ELSTER] Paquete de 186 bytes recibido completo. Decodificando nibbles...");

  // 3. Reordenar el paquete según la permutación del protocolo Elster A150
  for (int i = 0; i < 93; i++) {
    bytesOrdenados[i * 2]     = bytesRecibidos[93 + i];
    bytesOrdenados[i * 2 + 1] = bytesRecibidos[i];
  }

  // 4. Mapear cada byte a su valor nibble real (4 bits)
  for (int i = 0; i < numDatos; i++) {
    nibbles[i] = mapByteToNibble(bytesOrdenados[i]);
  }

  // 5. Decodificar magnitudes eléctricas según el mapa de memoria oficial de Elster A150
  // Energía Activa Importada (índices 10 a 3, BCD invertido)
  uint32_t val = 0;
  uint32_t mult = 1;
  for (int i = 10; i >= 3; i--) {
    val += mult * nibbles[i];
    mult *= 10;
  }
  data.energiaActivaImp = val; // En centésimas de kWh (0.01 kWh)

  // Energía Reactiva Importada (índices 30 a 23)
  val = 0;
  mult = 1;
  for (int i = 30; i >= 23; i--) {
    val += mult * nibbles[i];
    mult *= 10;
  }
  data.energiaReactivaImp = val;

  // Demanda Máxima Activa (índices 98 a 93)
  val = 0;
  mult = 1;
  for (int i = 98; i >= 93; i--) {
    val += mult * nibbles[i];
    mult *= 10;
  }
  data.maximaDemandaImp = val;

  // Corriente Fase A (índices 126 a 122)
  val = 0;
  mult = 1;
  for (int i = 126; i >= 122; i--) {
    val += mult * nibbles[i];
    mult *= 10;
  }
  data.corrienteA = (uint16_t)val; // Centésimas de Ampere (0.01 A)
  data.corrienteB = 0;
  data.corrienteC = 0;

  // Factor de Potencia Cosφ (índices 128 a 127)
  val = 0;
  mult = 1;
  for (int i = 128; i >= 127; i--) {
    val += mult * nibbles[i];
    mult *= 10;
  }
  data.cosphi = (uint8_t)val;

  // Voltaje Fase A (índices 132 a 130)
  val = 0;
  mult = 1;
  for (int i = 132; i >= 130; i--) {
    val += mult * nibbles[i];
    mult *= 10;
  }
  data.voltajeA = (uint16_t)val; // Volts entero (ej: 232V)
  data.voltajeB = 0;
  data.voltajeC = 0;



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
