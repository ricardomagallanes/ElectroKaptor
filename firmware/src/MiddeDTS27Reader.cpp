#include "MiddeDTS27Reader.h"
#include "MeterConfig.h"
#include "BoardConfig.h"

#define BIT_TIME_US 3333 // 300 baudios (1/300 = 3333 us por bit)

MiddeDTS27Reader::MiddeDTS27Reader(uint8_t rxPin, uint8_t txPin) 
  : _rxPin(rxPin), _txPin(txPin) {}

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

  // 7 Data bits
  for (int i = 0; i < 7; i++) {
    bool bitVal = (val >> i) & 1;
    digitalWrite(_txPin, bitVal ? HIGH : LOW);
    delayMicroseconds(BIT_TIME_US);
  }

  // Parity bit (Par)
  digitalWrite(_txPin, parity ? HIGH : LOW);
  delayMicroseconds(BIT_TIME_US);

  // Stop bit: HIGH
  digitalWrite(_txPin, HIGH);
  delayMicroseconds(BIT_TIME_US * 2);
}

void MiddeDTS27Reader::sendStringBitbang(const char* str) {
  while (*str) {
    // Parpadeo suave en LED 2 durante la transmisión del comando
    digitalWrite(LED_2_PIN, (digitalRead(LED_2_PIN) == LOW) ? HIGH : LOW);
    sendCharBitbang(*str++);
  }
}

int MiddeDTS27Reader::readCharBitbang(unsigned long timeoutMs) {
  unsigned long start = millis();

  while (millis() - start < timeoutMs) {
    // Parpadeo de búsqueda en LED 2
    digitalWrite(LED_2_PIN, ((millis() / 100) % 2) ? LOW : HIGH);

    // Detección de Start Bit (nivel LOW al recibir luz IR del medidor)
    if (digitalRead(_rxPin) == LOW) {
      delayMicroseconds(BIT_TIME_US / 2); // Ir al centro del bit de inicio
      if (digitalRead(_rxPin) != LOW) continue; // Validar que siga en LOW (descartar ruido)

      uint8_t val = 0;
      for (int i = 0; i < 7; i++) {
        delayMicroseconds(BIT_TIME_US); // Muestrear al centro de cada uno de los 7 bits
        if (digitalRead(_rxPin) == HIGH) {
          val |= (1 << i);
        }
      }
      
      // Saltear bit de paridad y bit de parada (3 tiempos de bit)
      delayMicroseconds(BIT_TIME_US * 3);
      return val & 0x7F;
    }
  }
  return -1;
}

#include "DebugSerial.h"

bool MiddeDTS27Reader::readMeter(MeterData &data, unsigned long timeoutMs) {
  memset(&data, 0, sizeof(MeterData));
  data.lecturaValida = false;
  data.tipoMedidor = 2; // Trifásico (MIDDE DTS27)
  data.estado = 0;      // Normal

  debugPrintln("\n[IR-IEC] Iniciando lectura IEC 62056-21 Modo C (MIDDE DTS27)...");
  debugPrintf("[IR-IEC] Pines: RX=%d, TX=%d | Velocidad: 300 baudios 7E1...\n", _rxPin, _txPin);

  begin(300);
  delay(200);

  // 1. Envío de comando Sign-on (/?!\r\n)
  debugPrintln("[IR-IEC] 1. Enviando comando Sign-on (/?!\r\n)...");
  sendStringBitbang("/?!\r\n");

  // 2. Captura de la respuesta de identificación (/XXX5...)
  unsigned long timeout = millis() + 3500;
  String idResponse = "";

  while (millis() < timeout) {
    int b = readCharBitbang(150);
    if (b >= 0) {
      char c = (char)(b & 0x7F);
      idResponse += c;
      if (c == '\n') break;
    }
  }

  if (idResponse.length() == 0) {
    debugPrintln("[IR-IEC] Alerta: Sin respuesta a Sign-on. (Verificar alineación de sonda óptica en PA3/PB10).");
    digitalWrite(LED_2_PIN, HIGH);
    for (int i = 0; i < 5; i++) {
      digitalWrite(LED_3_PIN, LOW);
      delay(100);
      digitalWrite(LED_3_PIN, HIGH);
      delay(100);
    }
    data.estado = 2; // Sin Lectura
    return false;
  }

  debugPrintf("[IR-IEC] ¡Identificación del Medidor Recibida!: %s", idResponse.c_str());

  // 3. Enviar ACK de solicitud de volcado de datos: ACK(0x06) + "000\r\n"
  delay(200);
  debugPrintln("[IR-IEC] 2. Enviando ACK (\\x06000\\r\\n) para solicitar registros OBIS...");
  sendCharBitbang(0x06);
  sendStringBitbang("000\r\n");

  // 4. Captura y parseo del bloque de registros OBIS
  debugPrintln("[IR-IEC] 3. Recibiendo y decodificando registros OBIS:");
  timeout = millis() + 8000;
  String obisBuffer = "";
  int lineCount = 0;

  while (millis() < timeout) {
    int b = readCharBitbang(150);
    if (b >= 0) {
      char c = (char)(b & 0x7F);
      debugPrintChar(c);
      obisBuffer += c;
      timeout = millis() + 2000; // Refrescar timeout dinámico mientras sigan entrando caracteres

      if (c == '\n') {
        lineCount++;
        String line = obisBuffer;
        line.trim();

        // Parseo de registros OBIS del medidor DTS27
        if (line.startsWith("1.0.32.7.0")) {
          int p1 = line.indexOf('(');
          int p2 = line.indexOf('*', p1);
          if (p1 != -1 && p2 != -1) {
            float vA = line.substring(p1 + 1, p2).toFloat();
            data.voltajeA = (uint16_t)vA;
          }
        }
        else if (line.startsWith("1.0.52.7.0")) {
          int p1 = line.indexOf('(');
          int p2 = line.indexOf('*', p1);
          if (p1 != -1 && p2 != -1) {
            float vB = line.substring(p1 + 1, p2).toFloat();
            data.voltajeB = (uint16_t)vB;
          }
        }
        else if (line.startsWith("1.0.72.7.0")) {
          int p1 = line.indexOf('(');
          int p2 = line.indexOf('*', p1);
          if (p1 != -1 && p2 != -1) {
            float vC = line.substring(p1 + 1, p2).toFloat();
            data.voltajeC = (uint16_t)vC;
          }
        }
        else if (line.startsWith("1.0.13.7.0")) {
          int p1 = line.indexOf('(');
          int p2 = line.indexOf(')', p1);
          if (p1 != -1 && p2 != -1) {
            float fp = line.substring(p1 + 1, p2).toFloat();
            data.cosphi = (uint8_t)(fp * 100.0f);
          }
        }
        else if (line.startsWith("1.0.1.8.0") || line.startsWith("1.0.15.8.0") || line.startsWith("1.0.1.8.1")) {
          int p1 = line.indexOf('(');
          int p2 = line.indexOf('*', p1);
          if (p1 != -1 && p2 != -1) {
            float eImp = line.substring(p1 + 1, p2).toFloat();
            data.energiaActivaImp = (uint32_t)(eImp * 100.0f);
          }
        }
        else if (line.startsWith("1.0.31.7.0")) {
          int p1 = line.indexOf('(');
          int p2 = line.indexOf('*', p1);
          if (p1 != -1 && p2 != -1) {
            float iA = line.substring(p1 + 1, p2).toFloat();
            data.corrienteA = (uint16_t)(iA * 100.0f);
          }
        }
        else if (line.startsWith("1.0.51.7.0")) {
          int p1 = line.indexOf('(');
          int p2 = line.indexOf('*', p1);
          if (p1 != -1 && p2 != -1) {
            float iB = line.substring(p1 + 1, p2).toFloat();
            data.corrienteB = (uint16_t)(iB * 100.0f);
          }
        }
        else if (line.startsWith("1.0.14.7.0")) {
          int p1 = line.indexOf('(');
          int p2 = line.indexOf('*', p1);
          if (p1 != -1 && p2 != -1) {
            float hz = line.substring(p1 + 1, p2).toFloat();
            data.frecuenciaMin = (uint16_t)(hz * 100.0f);
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
    data.lecturaValida = true;
    data.estado = 0;
    
    // Parpadeo rápido en LED 2 indicando lectura óptica exitosa
    for (int i = 0; i < 8; i++) {
      digitalWrite(LED_2_PIN, LOW);
      delay(40);
      digitalWrite(LED_2_PIN, HIGH);
      delay(40);
    }

    debugPrintln("\n==================================================");
    debugPrintln("=== ¡¡¡PARAMETROS DECODIFICADOS DEL MEDIDOR!!! ===");
    debugPrintln("==================================================");
    debugPrintf(" • ID Medidor               : %s", idResponse.c_str());
    debugPrintf(" • Voltaje Fase A           : %u V\n", data.voltajeA);
    debugPrintf(" • Voltaje Fase B           : %u V\n", data.voltajeB);
    debugPrintf(" • Voltaje Fase C           : %u V\n", data.voltajeC);
    debugPrintf(" • Corriente Fase A         : %.2f A\n", data.corrienteA / 100.0f);
    debugPrintf(" • Corriente Fase B         : %.2f A\n", data.corrienteB / 100.0f);
    debugPrintf(" • Factor de Potencia (Cos): %.2f\n", data.cosphi / 100.0f);
    debugPrintf(" • Frecuencia Red           : %.2f Hz\n", data.frecuenciaMin / 100.0f);
    debugPrintf(" • Energía Activa Importada : %.2f kWh\n", data.energiaActivaImp / 100.0f);
    debugPrintln("==================================================");
    return true;
  }

  // Lectura fallida / Sin respuesta: Apagar LED 2 y parpadear LED 3 de Error
  digitalWrite(LED_2_PIN, HIGH);
  for (int i = 0; i < 5; i++) {
    digitalWrite(LED_3_PIN, LOW);
    delay(100);
    digitalWrite(LED_3_PIN, HIGH);
    delay(100);
  }

  data.estado = 2; // Sin Lectura
  return false;
}
