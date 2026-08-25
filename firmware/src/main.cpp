#include <Arduino.h>
#include <Wire.h>
#include "MeterConfig.h"
#include "IMeterReader.h"
#include "MeterReaderFactory.h"
#include "BitPacker.h"
#include "LoRaWAN_Handler.h"

#ifdef HAS_OLED_DISPLAY
#include <U8g2lib.h>
// Inicialización del display OLED SSD1306 128x64 I2C para Heltec V3
U8G2_SSD1306_128X64_NONAME_F_SW_I2C u8g2(U8G2_R0, OLED_SCL_PIN, OLED_SDA_PIN, OLED_RST_PIN);
#endif

IMeterReader *meterReader = nullptr;
LoRaWANHandler lora;

MeterData currentData;
uint8_t payloadBuffer[32];
uint8_t msgCounter = 0;

void oledShowStatus(const char *header, const char *line1, const char *line2 = "", const char *line3 = "", const char *line4 = "") {
#ifdef HAS_OLED_DISPLAY
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.setDrawColor(1);
  
  u8g2.drawStr(0, 10, header);
  u8g2.drawLine(0, 12, 127, 12);
  
  if (line1 && strlen(line1) > 0) u8g2.drawStr(0, 24, line1);
  if (line2 && strlen(line2) > 0) u8g2.drawStr(0, 36, line2);
  if (line3 && strlen(line3) > 0) u8g2.drawStr(0, 48, line3);
  if (line4 && strlen(line4) > 0) u8g2.drawStr(0, 60, line4);
  
  u8g2.sendBuffer();
#endif
}

void initOLED() {
#ifdef HAS_OLED_DISPLAY
  // Encender línea Vext de alimentación del display (Heltec V3)
  pinMode(OLED_VEXT_PIN, OUTPUT);
  digitalWrite(OLED_VEXT_PIN, LOW); // LOW activa la alimentación
  delay(100);

  u8g2.begin();
  oledShowStatus(" ELECTROKAPTOR ", "Iniciando...", "Placa: Heltec V3", "Medidor: DTS27");
#endif
}

void updatePowerLed() {
#if defined(LED_POWER_PIN) && defined(POWER_SENSE_PIN)
  pinMode(POWER_SENSE_PIN, INPUT);
  if (digitalRead(POWER_SENSE_PIN) == HIGH) {
    digitalWrite(LED_POWER_PIN, HIGH);
  } else {
    digitalWrite(LED_POWER_PIN, LOW);
  }
#elif defined(LED_POWER_PIN)
  digitalWrite(LED_POWER_PIN, HIGH);
#endif
}

void initLeds() {
#ifdef LED_POWER_PIN
  pinMode(LED_POWER_PIN, OUTPUT);
  digitalWrite(LED_POWER_PIN, LOW);
#endif
#ifdef LED_FAIL_PIN
  pinMode(LED_FAIL_PIN, OUTPUT);
  digitalWrite(LED_FAIL_PIN, LOW);
#endif
#ifdef LED_TX_LORA_PIN
  pinMode(LED_TX_LORA_PIN, OUTPUT);
  digitalWrite(LED_TX_LORA_PIN, LOW);
#endif
#ifdef LED_UNUSED_PIN
  pinMode(LED_UNUSED_PIN, OUTPUT);
  digitalWrite(LED_UNUSED_PIN, LOW);
#endif

  // BARRIDO VISUAL DE ARRANQUE LED POR LED (D6 -> D5 -> D3 -> D4)
#if defined(LED_POWER_PIN) && defined(LED_FAIL_PIN) && defined(LED_TX_LORA_PIN) && defined(LED_UNUSED_PIN)
  // 1. D6 Verde (PB12)
  digitalWrite(LED_POWER_PIN, HIGH);   delay(400); digitalWrite(LED_POWER_PIN, LOW); delay(100);
  // 2. D5 Rojo (PB13)
  digitalWrite(LED_FAIL_PIN, HIGH);    delay(400); digitalWrite(LED_FAIL_PIN, LOW);  delay(100);
  // 3. D3 Rojo (PB14)
  digitalWrite(LED_TX_LORA_PIN, HIGH); delay(400); digitalWrite(LED_TX_LORA_PIN, LOW); delay(100);
  // 4. D4 Rojo (PB15)
  digitalWrite(LED_UNUSED_PIN, HIGH);  delay(400); digitalWrite(LED_UNUSED_PIN, LOW); delay(100);

  // Destello final de todos juntos (300 ms) y luego TODOS APAGADOS
  digitalWrite(LED_POWER_PIN, HIGH);
  digitalWrite(LED_FAIL_PIN, HIGH);
  digitalWrite(LED_TX_LORA_PIN, HIGH);
  digitalWrite(LED_UNUSED_PIN, HIGH);
  delay(300);
  digitalWrite(LED_POWER_PIN, LOW);
  digitalWrite(LED_FAIL_PIN, LOW);
  digitalWrite(LED_TX_LORA_PIN, LOW);
  digitalWrite(LED_UNUSED_PIN, LOW);
  delay(500);
#endif
}

void blinkFailLed(uint8_t count = 5, uint16_t speedMs = 150) {
#ifdef LED_FAIL_PIN
  for (uint8_t i = 0; i < count; i++) {
    digitalWrite(LED_FAIL_PIN, HIGH);
    delay(speedMs);
    digitalWrite(LED_FAIL_PIN, LOW);
    delay(speedMs);
  }
#endif
}

void blinkTxLed(uint8_t count = 3, uint16_t speedMs = 200) {
#ifdef LED_TX_LORA_PIN
  for (uint8_t i = 0; i < count; i++) {
    digitalWrite(LED_TX_LORA_PIN, HIGH);
    delay(speedMs);
    digitalWrite(LED_TX_LORA_PIN, LOW);
    delay(speedMs);
  }
#endif
}

void setup() {
  Serial.begin(SERIAL_DEBUG_BAUD);
  delay(2000);

  initOLED();
  initLeds();

  // Instanciar el lector según la configuración en MeterConfig.h y BoardConfig.h
  meterReader = MeterReaderFactory::createMeterReader(IR_RX_PIN, IR_TX_PIN);

  Serial.println("=================================================");
  Serial.println(" ElectroKaptor: Sistema de Telemetria LoRaWAN    ");
  Serial.print  (" Placa de Desarrollo: "); Serial.println(BOARD_NAME);
  Serial.print  (" Medidor Seleccionado: "); Serial.println(meterReader->getMeterName());
  Serial.println("=================================================");

  oledShowStatus(" ELECTROKAPTOR ", "Uniendo LoRaWAN...", "OTAA con TTN", meterReader->getMeterName());

  // Inicializar lector infrarrojo
  meterReader->begin(IR_DEFAULT_BAUD_RATE);

  // Parpadear LED Tx durante inicio / handshake OTAA
  blinkTxLed(4, 250);

  // Inicializar stack LoRaWAN
  lora.begin();

  // Si no está unido a la red TTN, reintentar Join OTAA
  if (!lora.joinOTAA()) {
    blinkFailLed(6, 150);
    oledShowStatus(" ELECTROKAPTOR ", "LoRa: FALLO JOIN", "Revisar Gateway AU915", "Reintentando Join...");
  } else {
    oledShowStatus(" ELECTROKAPTOR ", "LoRa: CONECTADO", "Listo para medir");
  }
}

void loop() {
  updatePowerLed();

  Serial.print("\n[IR] Intentando lectura del medidor ");
  Serial.print(meterReader->getMeterName());
  Serial.println("...");

  oledShowStatus(" LECTURA IR ", "Leyendo medidor...", meterReader->getMeterName());

  char l1[32] = "", l2[32] = "", l3[32] = "", l4[32] = "";

#ifdef LED_TX_LORA_PIN
  digitalWrite(LED_TX_LORA_PIN, HIGH); // Encender LED D3 durante consulta IR
#endif

  bool readOk = meterReader->readMeter(currentData, IR_DEFAULT_TIMEOUT_MS);

#ifdef LED_TX_LORA_PIN
  digitalWrite(LED_TX_LORA_PIN, LOW);
#endif

  if (readOk && currentData.lecturaValida) {
    Serial.println("[IR] Lectura exitosa de registros del medidor DTS27.");
    Serial.printf("  - Tensiones: Va=%u V | Vb=%u V | Vc=%u V\n", currentData.voltajeA, currentData.voltajeB, currentData.voltajeC);
    Serial.printf("  - Corrientes: Ia=%.2f A | Ib=%.2f A | Ic=%.2f A\n", currentData.corrienteA / 100.0, currentData.corrienteB / 100.0, currentData.corrienteC / 100.0);
    Serial.printf("  - CosPhi: %.2f | Frecuencia: %.2f Hz | Energía Imp: %.2f kWh\n",
                  currentData.cosphi / 100.0, currentData.frecuenciaMin / 100.0, currentData.energiaActivaImp / 100.0);

    // Destellar LED Verde (D6) 3 veces rápido para señalar LECTURA IR EXITOSA
#ifdef LED_POWER_PIN
    for (int i = 0; i < 3; i++) {
      digitalWrite(LED_POWER_PIN, LOW); delay(100);
      digitalWrite(LED_POWER_PIN, HIGH); delay(100);
    }
#endif

    // Pantalla 1 (4 seg): Tensiones trifásicas y Corrientes A/B
    snprintf(l1, sizeof(l1), "Va:%dV Vb:%dV Vc:%dV", currentData.voltajeA, currentData.voltajeB, currentData.voltajeC);
    snprintf(l2, sizeof(l2), "Ia:%.2fA  Ib:%.2fA", currentData.corrienteA / 100.0, currentData.corrienteB / 100.0);
    snprintf(l3, sizeof(l3), "FP:%.2f  Freq:%.2fHz", currentData.cosphi / 100.0, currentData.frecuenciaMin / 100.0);
    snprintf(l4, sizeof(l4), "ESTADO: DTS27 OK [1/3]");
    oledShowStatus(" MIDDE DTS27 TRIF ", l1, l2, l3, l4);
    lora.process(4000);

    // Pantalla 2 (4 seg): Frecuencia de Red, Corriente C y Factor de Potencia
    snprintf(l1, sizeof(l1), "Ic:%.2fA  FP:%.2f", currentData.corrienteC / 100.0, currentData.cosphi / 100.0);
    snprintf(l2, sizeof(l2), "Frecuencia: %.2f Hz", currentData.frecuenciaMin / 100.0);
    snprintf(l3, sizeof(l3), "E.ActImp: %.2fkWh", currentData.energiaActivaImp / 100.0);
    snprintf(l4, sizeof(l4), "ESTADO: DTS27 OK [2/3]");
    oledShowStatus(" MIDDE DTS27 TRIF ", l1, l2, l3, l4);
    lora.process(4000);

    // Pantalla 3 (4 seg): Energías Importada, Exportada y Potencias
    snprintf(l1, sizeof(l1), "E.Imp: %.2f kWh", currentData.energiaActivaImp / 100.0);
    snprintf(l2, sizeof(l2), "E.Exp: %.2f kWh", currentData.energiaActivaExp / 100.0);
    snprintf(l3, sizeof(l3), "Frec: %.2f Hz", currentData.frecuenciaMin / 100.0);
    snprintf(l4, sizeof(l4), "ESTADO: DTS27 OK [3/3]");
    oledShowStatus(" MIDDE DTS27 TRIF ", l1, l2, l3, l4);
    lora.process(4000);
  } else {
    Serial.println("[IR] Alerta: No se pudo establecer sincronización con el medidor. Enviando mensaje de estado.");
    currentData.estado = 2; // Sin Lectura / Error IR
    currentData.tipoMedidor = 2; // Trifásico DTS27
    blinkFailLed(3, 200);
    oledShowStatus(" ALERTA IR ", "Sin Sincronismo", "Verifique sonda IR", "DTS27 DESCONECTADO");
  }

  // Determinar número de mensajes soportados según el tipo de medidor:
  // Monofásico o Trifásico (DTS27) -> Mensajes 0 y 1
  // Grandes Clientes -> Mensajes 0, 1, 2 y 3
  uint8_t maxMsgs = (currentData.tipoMedidor == 3) ? 4 : 2;
  if (msgCounter >= maxMsgs) {
    msgCounter = 0;
  }

  uint8_t len = 0;
  if (msgCounter == 0) {
    len = BitPacker::packMessage0(currentData, payloadBuffer);
  } else if (msgCounter == 1) {
    len = BitPacker::packMessage1(currentData, payloadBuffer);
  } else if (msgCounter == 2) {
    len = BitPacker::packMessage2(currentData, payloadBuffer);
  } else if (msgCounter == 3) {
    len = BitPacker::packMessage3(currentData, payloadBuffer);
  }

  uint8_t txBuffer[64];
  uint8_t txLen = 0;

#if SELECTED_PAYLOAD_FORMAT == PAYLOAD_FORMAT_ASCII_HEX
  txLen = BitPacker::convertToAsciiHex(payloadBuffer, len, txBuffer);
#else
  memcpy(txBuffer, payloadBuffer, len);
  txLen = len;
#endif

  // Intentar envío de paquete por LoRaWAN (y reintentar Join OTAA si aún no está unido)
  bool sentOk = lora.sendPayload(txBuffer, txLen, 10);
  if (!sentOk) {
    Serial.println("[LoRaWAN] Reintentando Join OTAA en este ciclo...");
    if (lora.joinOTAA()) {
      sentOk = lora.sendPayload(txBuffer, txLen, 10);
    }
  }

#ifdef LED_TX_LORA_PIN
  digitalWrite(LED_TX_LORA_PIN, LOW);
#endif

  if (sentOk) {
    Serial.println("[LoRaWAN] Telemetria enviada con éxito.");
    blinkTxLed(3, 100);
    oledShowStatus(" TELEMETRIA LORA ", "DATOS ENVIADOS", "Enviado a TTN OK", "DTS27 CONECTADO");
    lora.process(2000);
  } else {
    Serial.println("[LoRaWAN] ALERTA CRITICA: Falló el envío de datos por LoRaWAN.");
    blinkFailLed(4, 150);
    oledShowStatus(" ERROR LORAWAN ", "Fallo Envio Paquete", "Revisar Gateway AU915", "REINTENTANDO...");
    lora.process(3000);
  }

  if (!sentOk) {
    blinkFailLed(5, 150);
  }

  // Incrementar contador respetando el máximo de mensajes del medidor actual
  msgCounter = (msgCounter + 1) % maxMsgs;

  // Esperar el intervalo configurado antes del siguiente ciclo de telemetría
  Serial.print("[LOOP] Entrando en reposo hasta el siguiente ciclo de telemetria (");
  Serial.print(TELEMETRY_INTERVAL_MS / 1000);
  Serial.println(" seg)...");
  lora.process(TELEMETRY_INTERVAL_MS);
}
