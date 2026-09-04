#include "MiddeDTS27Reader.h"
#include "MeterConfig.h"
#include "BoardConfig.h"
#include "DebugSerial.h"

#define BIT_TIME_US 3333 // 300 baudios (1/300 = 3333.33 us por bit)

MiddeDTS27Reader::MiddeDTS27Reader(uint8_t rxPin, uint8_t txPin) 
  : BaseMeterReader(rxPin, txPin) {}

void MiddeDTS27Reader::begin(unsigned long baudRate) {
  pinMode(_txPin, OUTPUT);
  digitalWrite(_txPin, HIGH); // Idle HIGH para TX Infrarrojo
  pinMode(_rxPin, INPUT_PULLUP);
}

void MiddeDTS27Reader::sendCharBitbang(char c) {
  uint8_t val = (uint8_t)c & 0x7F;
  uint8_t parity = 0;
  for (int i = 0; i < 7; i++) {
    if (val & (1 << i)) parity ^= 1;
  }

  // Start bit: LOW
  digitalWrite(_txPin, LOW);
  delayMicroseconds(BIT_TIME_US);

  // 7 Data bits (LSB first)
  for (int i = 0; i < 7; i++) {
    digitalWrite(_txPin, (val & (1 << i)) ? HIGH : LOW);
    delayMicroseconds(BIT_TIME_US);
  }

  // Even Parity bit
  digitalWrite(_txPin, parity ? HIGH : LOW);
  delayMicroseconds(BIT_TIME_US);

  // Stop bit: HIGH
  digitalWrite(_txPin, HIGH);
  delayMicroseconds(BIT_TIME_US * 2); // 2 stop bits para máxima compatibilidad
}

void MiddeDTS27Reader::sendStringBitbang(const char* str) {
  while (*str) {
    sendCharBitbang(*str++);
  }
}

int MiddeDTS27Reader::readCharBitbang(unsigned long timeoutMs) {
  unsigned long start = millis();

  while (millis() - start < timeoutMs) {
    // Detección precisa de Start Bit (flanco descendente HIGH -> LOW)
    if (digitalRead(_rxPin) == LOW) {
      notifyOpticalActivity();

      // Ir al 50% del bit de inicio para validar
      delayMicroseconds(BIT_TIME_US / 2);
      if (digitalRead(_rxPin) != LOW) continue; // Falsa alarma / ruido

      uint8_t val = 0;
      for (int i = 0; i < 7; i++) {
        delayMicroseconds(BIT_TIME_US); // Muestrear al centro de cada uno de los 7 bits
        if (digitalRead(_rxPin) == HIGH) {
          val |= (1 << i);
        }
      }
      
      // Saltar bit de paridad y avanzar al bit de parada
      delayMicroseconds(BIT_TIME_US);
      delayMicroseconds(BIT_TIME_US);

      // Aguardar línea en estado de reposo (HIGH)
      unsigned long waitStop = micros();
      while (digitalRead(_rxPin) == LOW && (micros() - waitStop < BIT_TIME_US * 2)) {
        // Pausa hasta que la línea retorne a IDLE
      }

      return val & 0x7F;
    }
  }
  return -1;
}

bool MiddeDTS27Reader::performOpticalRead(MeterData &data, unsigned long timeoutMs) {
  data.tipoMedidor = 2; // Trifásico (MIDDE DTS27)

  debugPrintln("\n[IR-IEC] Iniciando lectura IEC 62056-21 Modo C (MIDDE DTS27)...");
  debugPrintf("[IR-IEC] Pines: RX=%d, TX=%d | Velocidad: 300 baudios 7E1...\n", _rxPin, _txPin);

  begin(300);
  delay(300);

  // Limpiar buffer de entrada
  while (readCharBitbang(50) >= 0);

  // 1. Envío de comando Sign-on (/?!\r\n)
  debugPrintln("[IR-IEC] 1. Enviando comando Sign-on (/?!\\r\\n)...");
  sendStringBitbang("/?!\r\n");

  // 2. Captura de la respuesta de identificación (/XXX5...)
  unsigned long startWait = millis();
  String idResponse = "";

  while (millis() - startWait < 4500) {
    notifyOpticalActivity();
    int b = readCharBitbang(idResponse.length() == 0 ? 500 : 300);
    if (b >= 0) {
      char c = (char)(b & 0x7F);
      idResponse += c;
      if (c == '\n') break;
    } else if (idResponse.length() > 0) {
      break;
    }
  }

  if (idResponse.length() == 0) {
    debugPrintln("[IR-IEC] Alerta: Sin respuesta a Sign-on. (Verificar alineación de sonda óptica en PA3/PB10).");
    return false;
  }

  debugPrintf("[IR-IEC] ¡Identificación del Medidor Recibida!: %s\n", idResponse.c_str());

  // 3. Enviar ACK de solicitud de volcado de datos: ACK(0x06) + "000\r\n"
  delay(150);
  debugPrintln("[IR-IEC] 2. Enviando ACK (\\x06000\\r\\n) para solicitar registros OBIS...");
  sendCharBitbang(0x06);
  sendStringBitbang("000\r\n");

  // 4. Captura y parseo del bloque de registros OBIS
  debugPrintln("[IR-IEC] 3. Recibiendo y decodificando registros OBIS:");
  unsigned long obisTimeout = millis() + 12000;
  String obisBuffer = "";
  int lineCount = 0;

  while (millis() < obisTimeout) {
    notifyOpticalActivity();
    int b = readCharBitbang(400);
    if (b >= 0) {
      char c = (char)(b & 0x7F);
      debugPrintChar(c);
      obisBuffer += c;
      obisTimeout = millis() + 3000; // Refrescar timeout dinámico mientras sigan entrando caracteres

      if (c == '\n') {
        String line = obisBuffer;
        line.trim();
        lineCount++;

        // Parseo flexible de registros OBIS del medidor DTS27 (formato completo y corto)
        if (line.startsWith("1.0.32.7.0") || line.startsWith("32.7.0")) {
          int p1 = line.indexOf('(');
          int p2 = line.indexOf('*', p1);
          if (p1 == -1) p2 = line.indexOf(')', p1);
          if (p1 != -1 && p2 != -1) {
            float vA = line.substring(p1 + 1, p2).toFloat();
            data.voltajeA = (uint16_t)vA;
          }
        }
        else if (line.startsWith("1.0.52.7.0") || line.startsWith("52.7.0")) {
          int p1 = line.indexOf('(');
          int p2 = line.indexOf('*', p1);
          if (p1 == -1) p2 = line.indexOf(')', p1);
          if (p1 != -1 && p2 != -1) {
            float vB = line.substring(p1 + 1, p2).toFloat();
            data.voltajeB = (uint16_t)vB;
          }
        }
        else if (line.startsWith("1.0.72.7.0") || line.startsWith("72.7.0")) {
          int p1 = line.indexOf('(');
          int p2 = line.indexOf('*', p1);
          if (p1 == -1) p2 = line.indexOf(')', p1);
          if (p1 != -1 && p2 != -1) {
            float vC = line.substring(p1 + 1, p2).toFloat();
            data.voltajeC = (uint16_t)vC;
          }
        }
        else if (line.startsWith("1.0.31.7.0") || line.startsWith("31.7.0")) {
          int p1 = line.indexOf('(');
          int p2 = line.indexOf('*', p1);
          if (p1 == -1) p2 = line.indexOf(')', p1);
          if (p1 != -1 && p2 != -1) {
            float iA = line.substring(p1 + 1, p2).toFloat();
            data.corrienteA = (uint16_t)(iA * 100.0f);
          }
        }
        else if (line.startsWith("1.0.51.7.0") || line.startsWith("51.7.0")) {
          int p1 = line.indexOf('(');
          int p2 = line.indexOf('*', p1);
          if (p1 == -1) p2 = line.indexOf(')', p1);
          if (p1 != -1 && p2 != -1) {
            float iB = line.substring(p1 + 1, p2).toFloat();
            data.corrienteB = (uint16_t)(iB * 100.0f);
          }
        }
        else if (line.startsWith("1.0.71.7.0") || line.startsWith("71.7.0")) {
          int p1 = line.indexOf('(');
          int p2 = line.indexOf('*', p1);
          if (p1 == -1) p2 = line.indexOf(')', p1);
          if (p1 != -1 && p2 != -1) {
            float iC = line.substring(p1 + 1, p2).toFloat();
            data.corrienteC = (uint16_t)(iC * 100.0f);
          }
        }
        else if (line.startsWith("1.0.13.7.0") || line.startsWith("13.7.0")) {
          int p1 = line.indexOf('(');
          int p2 = line.indexOf(')', p1);
          if (p1 != -1 && p2 != -1) {
            float fp = line.substring(p1 + 1, p2).toFloat();
            data.cosphi = (uint8_t)(fp * 100.0f);
          }
        }
        else if (line.startsWith("1.0.14.7.0") || line.startsWith("14.7.0")) {
          int p1 = line.indexOf('(');
          int p2 = line.indexOf('*', p1);
          if (p1 == -1) p2 = line.indexOf(')', p1);
          if (p1 != -1 && p2 != -1) {
            float hz = line.substring(p1 + 1, p2).toFloat();
            data.frecuenciaMin = (uint16_t)(hz * 100.0f);
          }
        }
        else if (line.startsWith("1.0.1.8.0") || line.startsWith("1.8.0") || line.startsWith("1.0.15.8.0") || line.startsWith("1.0.1.8.1")) {
          int p1 = line.indexOf('(');
          int p2 = line.indexOf('*', p1);
          if (p1 == -1) p2 = line.indexOf(')', p1);
          if (p1 != -1 && p2 != -1) {
            float eImp = line.substring(p1 + 1, p2).toFloat();
            data.energiaActivaImp = (uint32_t)(eImp * 100.0f);
          }
        }
        else if (line.startsWith("1.0.2.8.0") || line.startsWith("2.8.0")) {
          int p1 = line.indexOf('(');
          int p2 = line.indexOf('*', p1);
          if (p1 == -1) p2 = line.indexOf(')', p1);
          if (p1 != -1 && p2 != -1) {
            float eExp = line.substring(p1 + 1, p2).toFloat();
            data.energiaActivaExp = (uint32_t)(eExp * 100.0f);
          }
        }
        else if (line.startsWith("1.0.3.8.0") || line.startsWith("3.8.0")) {
          int p1 = line.indexOf('(');
          int p2 = line.indexOf('*', p1);
          if (p1 == -1) p2 = line.indexOf(')', p1);
          if (p1 != -1 && p2 != -1) {
            float qImp = line.substring(p1 + 1, p2).toFloat();
            data.energiaReactivaImp = (uint32_t)(qImp * 100.0f);
          }
        }
        else if (line.startsWith("1.0.4.8.0") || line.startsWith("4.8.0")) {
          int p1 = line.indexOf('(');
          int p2 = line.indexOf('*', p1);
          if (p1 == -1) p2 = line.indexOf(')', p1);
          if (p1 != -1 && p2 != -1) {
            float qExp = line.substring(p1 + 1, p2).toFloat();
            data.energiaReactivaExp = (uint32_t)(qExp * 100.0f);
          }
        }
        else if (line.startsWith("1.0.1.6.0") || line.startsWith("1.6.0")) {
          int p1 = line.indexOf('(');
          int p2 = line.indexOf('*', p1);
          if (p1 == -1) p2 = line.indexOf(')', p1);
          if (p1 != -1 && p2 != -1) {
            float pMax = line.substring(p1 + 1, p2).toFloat();
            data.maximaDemandaImp = (uint32_t)(pMax * 100.0f);
          }
        }

        obisBuffer = "";
      }

      if (c == '!') { // Fin de trama IEC 62056-21
        debugPrintln("\n[IR-IEC] Fin de trama detectado (!).");
        break;
      }
    }
  }

  if (lineCount > 0 || data.voltajeA > 0 || data.energiaActivaImp > 0 || idResponse.length() > 0) {
    debugPrintln("\n==================================================");
    debugPrintln("=== ¡¡¡PARAMETROS DECODIFICADOS DEL MEDIDOR!!! ===");
    debugPrintln("==================================================");
    debugPrintf(" • ID Medidor               : %s\n", idResponse.c_str());
    debugPrintf(" • Voltaje Fase A           : %u V\n", data.voltajeA);
    debugPrintf(" • Voltaje Fase B           : %u V\n", data.voltajeB);
    debugPrintf(" • Voltaje Fase C           : %u V\n", data.voltajeC);
    debugPrintf(" • Corriente Fase A         : %.2f A\n", data.corrienteA / 100.0f);
    debugPrintf(" • Corriente Fase B         : %.2f A\n", data.corrienteB / 100.0f);
    debugPrintf(" • Corriente Fase C         : %.2f A\n", data.corrienteC / 100.0f);
    debugPrintf(" • Factor de Potencia (Cos): %.2f\n", data.cosphi / 100.0f);
    debugPrintf(" • Frecuencia Red           : %.2f Hz\n", data.frecuenciaMin / 100.0f);
    debugPrintf(" • Energía Activa Importada : %.2f kWh\n", data.energiaActivaImp / 100.0f);
    debugPrintf(" • Energía Activa Exportada : %.2f kWh\n", data.energiaActivaExp / 100.0f);
    debugPrintf(" • Demanda Máxima Activa    : %.2f kW\n", data.maximaDemandaImp / 100.0f);
    debugPrintln("==================================================");
    return true;
  }

  return false;
}
