#include "MiddeDTS27Reader.h"
#include "MeterConfig.h"

#if defined(ESP32)
MiddeDTS27Reader::MiddeDTS27Reader(uint8_t rxPin, uint8_t txPin) 
  : _rxPin(rxPin), _txPin(txPin), _irSerial(1) {}
#elif defined(ARDUINO_ARCH_STM32)
MiddeDTS27Reader::MiddeDTS27Reader(uint8_t rxPin, uint8_t txPin) 
  : _rxPin(rxPin), _txPin(txPin), _irSerial(USART1) {}
#else
MiddeDTS27Reader::MiddeDTS27Reader(uint8_t rxPin, uint8_t txPin) 
  : _rxPin(rxPin), _txPin(txPin), _irSerial(1) {}
#endif

void MiddeDTS27Reader::begin(unsigned long baudRate) {
  pinMode(_rxPin, INPUT_PULLUP);
#if defined(ESP32)
  _irSerial.begin(300, SERIAL_7E1, _rxPin, _txPin, false);
#else
  _irSerial.begin(300, SERIAL_7E1);
#endif
}

uint8_t MiddeDTS27Reader::mapByteToNibble(uint8_t val) {
  return 0; // Método heredado sin uso en protocolo ASCII OBIS
}

bool MiddeDTS27Reader::detectSyncSequence(unsigned long timeoutMs) {
  return true; // Reemplazado por protocolo IEC Handshake
}

bool MiddeDTS27Reader::readMeter(MeterData &data, unsigned long timeoutMs) {
  memset(&data, 0, sizeof(MeterData));
  data.lecturaValida = false;
  data.tipoMedidor = 2; // Trifásico (MIDDE DTS27)
  data.estado = 0;      // Normal

  Serial.println("\n[IR-IEC] Iniciando lectura IEC 62056-21 Modo C (MIDDE DTS27)...");
  Serial.printf("[IR-IEC] Pines: RX=%d, TX=%d | Velocidad: 300 baudios 7E1...\n", _rxPin, _txPin);

  pinMode(_rxPin, INPUT_PULLUP);
#if defined(ESP32)
  _irSerial.begin(300, SERIAL_7E1, _rxPin, _txPin, false);
#else
  _irSerial.begin(300, SERIAL_7E1);
#endif
  delay(100);

  // Limpiar buffer serie de entradas previas
  while (_irSerial.available()) _irSerial.read();

  // 1. Envío de comando Sign-on (/?!\r\n)
  Serial.println("[IR-IEC] 1. Enviando comando Sign-on (/?!\r\n)...");
  _irSerial.print("/?!\r\n");
  _irSerial.flush();

  // 2. Captura de la respuesta de identificación (/SCZ5...)
  unsigned long timeout = millis() + 3500;
  String idResponse = "";

  while (millis() < timeout) {
    if (_irSerial.available()) {
      char c = (char)(_irSerial.read() & 0x7F); // Enmascarar bit de paridad 7E1
      idResponse += c;
      if (c == '\n') break;
    }
  }

  if (idResponse.length() == 0) {
    Serial.println("[IR-IEC] Alerta: Sin respuesta a Sign-on. (Verificar orientación de sonda u opacidad).");
    data.estado = 2; // Sin Lectura
    return false;
  }

  Serial.printf("[IR-IEC] ¡Identificación recibida!: %s", idResponse.c_str());

  // 3. Enviar ACK de solicitud de volcado de datos: ACK(0x06) + "000\r\n"
  delay(200);
  Serial.println("[IR-IEC] 2. Enviando ACK (\x06000\r\n) para solicitar registros OBIS...");
  _irSerial.write(0x06);
  _irSerial.print("000\r\n");
  _irSerial.flush();

  // 4. Captura y parseo del bloque de registros OBIS
  Serial.println("[IR-IEC] 3. Recibiendo trama de registros OBIS:");
  timeout = millis() + 8000;
  String obisBuffer = "";
  int lineCount = 0;

  while (millis() < timeout) {
    if (_irSerial.available()) {
      char c = (char)(_irSerial.read() & 0x7F);
      Serial.print(c);
      obisBuffer += c;
      timeout = millis() + 2000; // Refrescar timeout dinámico mientras sigan entrando caracteres

      if (c == '\n') {
        lineCount++;
        // Analizar la línea completa acumulada
        String line = obisBuffer;
        line.trim();

        // 1.0.32.7.0*255(233.5*V) -> Tensión Fase A
        if (line.startsWith("1.0.32.7.0")) {
          int p1 = line.indexOf('(');
          int p2 = line.indexOf('*', p1);
          if (p1 != -1 && p2 != -1) {
            float vA = line.substring(p1 + 1, p2).toFloat();
            data.voltajeA = (uint16_t)vA; // Guardar el valor entero en Voltios (ej: 233.5 V -> 233 V)
          }
        }
        // 1.0.52.7.0*255(233.5*V) -> Tensión Fase B
        else if (line.startsWith("1.0.52.7.0")) {
          int p1 = line.indexOf('(');
          int p2 = line.indexOf('*', p1);
          if (p1 != -1 && p2 != -1) {
            float vB = line.substring(p1 + 1, p2).toFloat();
            data.voltajeB = (uint16_t)vB;
          }
        }
        // 1.0.72.7.0*255(233.5*V) -> Tensión Fase C
        else if (line.startsWith("1.0.72.7.0")) {
          int p1 = line.indexOf('(');
          int p2 = line.indexOf('*', p1);
          if (p1 != -1 && p2 != -1) {
            float vC = line.substring(p1 + 1, p2).toFloat();
            data.voltajeC = (uint16_t)vC;
          }
        }
        // 1.0.13.7.0*255(1.000) -> Factor de Potencia (cosphi)
        else if (line.startsWith("1.0.13.7.0")) {
          int p1 = line.indexOf('(');
          int p2 = line.indexOf(')', p1);
          if (p1 != -1 && p2 != -1) {
            float fp = line.substring(p1 + 1, p2).toFloat();
            data.cosphi = (uint8_t)(fp * 100.0f); // Guardar en centésimas (ej 1.00 -> 100)
          }
        }
        // 1.0.1.8.0*255(00012345.67*kWh) o 1.0.15.8.0 -> Energía Activa Importada Total
        else if (line.startsWith("1.0.1.8.0") || line.startsWith("1.0.15.8.0") || line.startsWith("1.0.1.8.1")) {
          int p1 = line.indexOf('(');
          int p2 = line.indexOf('*', p1);
          if (p1 != -1 && p2 != -1) {
            float eImp = line.substring(p1 + 1, p2).toFloat();
            data.energiaActivaImp = (uint32_t)(eImp * 100.0f); // Guardar en centésimas (kWh * 100)
          }
        }
        // 1.0.31.7.0*255(5.20*A) -> Corriente Fase A
        else if (line.startsWith("1.0.31.7.0")) {
          int p1 = line.indexOf('(');
          int p2 = line.indexOf('*', p1);
          if (p1 != -1 && p2 != -1) {
            float iA = line.substring(p1 + 1, p2).toFloat();
            data.corrienteA = (uint16_t)(iA * 100.0f);
          }
        }
        // 1.0.51.7.0*255(0.0*A) -> Corriente Fase B
        else if (line.startsWith("1.0.51.7.0")) {
          int p1 = line.indexOf('(');
          int p2 = line.indexOf('*', p1);
          if (p1 != -1 && p2 != -1) {
            float iB = line.substring(p1 + 1, p2).toFloat();
            data.corrienteB = (uint16_t)(iB * 100.0f);
          }
        }
        // 1.0.14.7.0*255(49.99*Hz) -> Frecuencia
        else if (line.startsWith("1.0.14.7.0")) {
          int p1 = line.indexOf('(');
          int p2 = line.indexOf('*', p1);
          if (p1 != -1 && p2 != -1) {
            float hz = line.substring(p1 + 1, p2).toFloat();
            data.frecuenciaMin = (uint16_t)(hz * 100.0f); // Guardar en centésimas de Hz
          }
        }
        // 1.0.2.8.0*255(0.00*kWh) -> Energía Activa Exportada Total
        else if (line.startsWith("1.0.2.8.0")) {
          int p1 = line.indexOf('(');
          int p2 = line.indexOf('*', p1);
          if (p1 != -1 && p2 != -1) {
            float eExp = line.substring(p1 + 1, p2).toFloat();
            data.energiaActivaExp = (uint32_t)(eExp * 100.0f);
          }
        }

        obisBuffer = ""; // Resetear buffer de línea
      }

      if (c == '!') { // Exclamación indica fin del mensaje IEC 62056-21
        Serial.println("\n[IR-IEC] Fin de trama detectado (!).");
        break;
      }
    }
  }

  if (lineCount > 0 || data.voltajeA > 0 || data.energiaActivaImp > 0) {
    data.lecturaValida = true;
    data.estado = 0;
    Serial.println("\n[IR-IEC] ¡¡¡LECTURA IEC OBIS DECODI FICADA Y PROCESADA CON ÉXITO!!!");
    Serial.printf("[IR-IEC] Tensión A: %.1f V | Tensión B: %.1f V | FP: %.2f | Energía Activa: %.2f kWh\n",
                  data.voltajeA / 10.0f, data.voltajeB / 10.0f, data.cosphi / 100.0f, data.energiaActivaImp / 100.0f);
    return true;
  }

  data.estado = 2; // Sin Lectura
  return false;
}
