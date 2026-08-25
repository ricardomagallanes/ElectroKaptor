#include "ElsterA150Reader.h"

#if defined(ESP32)
ElsterA150Reader::ElsterA150Reader(uint8_t rxPin, uint8_t txPin) 
  : _rxPin(rxPin), _txPin(txPin), _irSerial(1) {}
#elif defined(ARDUINO_ARCH_STM32)
ElsterA150Reader::ElsterA150Reader(uint8_t rxPin, uint8_t txPin) 
  : _rxPin(rxPin), _txPin(txPin), _irSerial(PA10, PA9) {}
#else
ElsterA150Reader::ElsterA150Reader(uint8_t rxPin, uint8_t txPin) 
  : _rxPin(rxPin), _txPin(txPin), _irSerial(1) {}
#endif

void ElsterA150Reader::begin(unsigned long baudRate) {
#if defined(ESP32)
  _irSerial.begin(baudRate, SERIAL_8N1, _rxPin, _txPin);
#else
  _irSerial.begin(baudRate, SERIAL_8N1);
#endif
}

uint8_t ElsterA150Reader::mapByteToNibble(uint8_t val) {
  switch (val) {
    case 85:  return 0x0;
    case 87:  return 0x1;
    case 93:  return 0x2;
    case 95:  return 0x3;
    case 117: return 0x4;
    case 119: return 0x5;
    case 125: return 0x6;
    case 127: return 0x7;
    case 213: return 0x8;
    case 215: return 0x9;
    case 221: return 0xA;
    case 223: return 0xB;
    case 245: return 0xC;
    case 247: return 0xD;
    case 253: return 0xE;
    case 255: return 0xF;
    default:  return 0x0;
  }
}

bool ElsterA150Reader::detectSyncSequence(unsigned long timeoutMs) {
  const uint8_t syncPattern[8] = {0x57, 0x55, 0x55, 0x55, 0x7F, 0x77, 0x5D, 0x55};
  uint8_t patternIdx = 0;
  unsigned long start = millis();

  while (millis() - start < timeoutMs) {
    if (_irSerial.available()) {
      uint8_t b = _irSerial.read();
      if (b == syncPattern[patternIdx]) {
        patternIdx++;
        if (patternIdx == 8) {
          return true; // Cabecera detectada
        }
      } else {
        patternIdx = (b == syncPattern[0]) ? 1 : 0;
      }
    }
  }
  return false;
}

bool ElsterA150Reader::readMeter(MeterData &data, unsigned long timeoutMs) {
  memset(&data, 0, sizeof(MeterData));
  data.lecturaValida = false;
  data.tipoMedidor = 3; // Elster A150 Grandes Clientes por defecto
  data.estado = 0;      // Normal

  if (!detectSyncSequence(timeoutMs)) {
    data.estado = 2; // Sin Lectura
    return false;
  }

  const uint16_t numDatos = 186;
  uint8_t bytesRecibidos[numDatos];
  uint8_t bytesOrdenados[numDatos];
  uint8_t nibbles[numDatos];

  unsigned long startRead = millis();
  uint16_t count = 0;
  while (count < (numDatos - 8) && (millis() - startRead < 3000)) {
    if (_irSerial.available()) {
      bytesRecibidos[count + 8] = _irSerial.read();
      count++;
    }
  }

  if (count < (numDatos - 8)) {
    data.estado = 2; // Incompleto
    return false;
  }

  // Intercambio de bytes alto/bajo (swap)
  for (uint8_t i = 8; i < numDatos; i += 2) {
    bytesOrdenados[i]     = bytesRecibidos[i + 1];
    bytesOrdenados[i + 1] = bytesRecibidos[i];
  }

  // Mapeo a nibbles BCD
  for (int i = 0; i < numDatos; i++) {
    nibbles[i] = mapByteToNibble(bytesOrdenados[i]);
  }

  // Mapeo de campos
  uint32_t val = 0;
  uint32_t mult = 1;

  // Máxima demanda importada
  for (int i = 179; i >= 177; i--) {
    val += mult * nibbles[i];
    mult *= 10;
  }
  data.maximaDemandaImp = val * 10; // kW * 100

  // Energía activa importada (índices 69 a 62)
  val = 0;
  mult = 1;
  for (int i = 69; i >= 62; i--) {
    val += mult * nibbles[i];
    mult *= 10;
  }
  data.energiaActivaImp = val; // kWh * 100

  // Corriente A (índices 125 a 123)
  val = 0;
  mult = 1;
  for (int i = 125; i >= 123; i--) {
    val += mult * nibbles[i];
    mult *= 10;
  }
  data.corrienteA = (val >> 2) * 10; // A * 100

  // Energía reactiva importada (índices 80 a 77)
  val = 0;
  mult = 1;
  for (int i = 80; i >= 77; i--) {
    val += mult * nibbles[i];
    mult *= 10;
  }
  data.energiaReactivaImp = (val << 10);

  // Factor de potencia (índices 116 a 115)
  val = 0;
  mult = 1;
  for (int i = 116; i >= 115; i--) {
    val += mult * nibbles[i];
    mult *= 10;
  }
  data.cosphi = val;

  // Voltaje A (índices 132 a 130)
  val = 0;
  mult = 1;
  for (int i = 132; i >= 130; i--) {
    val += mult * nibbles[i];
    mult *= 10;
  }
  data.voltajeA = val; // Volts entero (1V)

  data.bateria = 95;
  data.temperatura = 25;
  data.frecuenciaMin = 4995;
  data.frecuenciaMax = 5005;

  data.lecturaValida = true;
  return true;
}
