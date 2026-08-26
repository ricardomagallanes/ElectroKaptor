#include <Arduino.h>
#include "BoardConfig.h"
#include "MeterConfig.h"
#include "MiddeDTS27Reader.h"
#include "Credentials.h"

#define STATUS_LED_PIN 13

extern "C" __attribute__((used)) void HardFault_Handler(void) { NVIC_SystemReset(); }
extern "C" __attribute__((used)) void BusFault_Handler(void) { NVIC_SystemReset(); }
extern "C" __attribute__((used)) void UsageFault_Handler(void) { NVIC_SystemReset(); }
extern "C" __attribute__((used)) void ADC1_2_IRQHandler(void) {}

static const uint8_t devEuiBytes[] = LORAWAN_DEV_EUI;
static const uint8_t appEuiBytes[] = LORAWAN_APP_EUI;
static const uint8_t appKeyBytes[] = LORAWAN_APP_KEY;

// Variables de estado global en RAM
volatile uint32_t g_meterReadValid = 0;
volatile uint32_t g_energiaActiva = 0;

volatile uint32_t g_loraJoined = 0;
volatile uint32_t g_joinAttempts = 0;

void sendAtCmdHw(const char* cmd) {
  Serial1.print(cmd);
  Serial1.print("\r\n");
  delay(100);
}

void bytesToHexStr(const uint8_t* src, uint8_t len, char* dst) {
  const char hexChars[] = "0123456789ABCDEF";
  for (uint8_t i = 0; i < len; i++) {
    dst[i * 2]     = hexChars[(src[i] >> 4) & 0x0F];
    dst[i * 2 + 1] = hexChars[src[i] & 0x0F];
  }
  dst[len * 2] = '\0';
}

void configureRak3172Hw() {
  pinMode(PA1, OUTPUT);
  digitalWrite(PA1, LOW);  delay(50);
  digitalWrite(PA1, HIGH); delay(250);

  Serial1.setTx(PB6);
  Serial1.setRx(PB7);
  Serial1.begin(115200);
  delay(100);

  Serial1.print("\r\n"); delay(100);

  char devEuiStr[17], appEuiStr[17], appKeyStr[33], cmdBuf[128];
  bytesToHexStr(devEuiBytes, 8, devEuiStr);
  bytesToHexStr(appEuiBytes, 8, appEuiStr);
  bytesToHexStr(appKeyBytes, 16, appKeyStr);

  sendAtCmdHw("AT+NWM=1");
  sendAtCmdHw("AT+BAND=6");    // AU915
  sendAtCmdHw("AT+MASK=0002");  // FSB2 (Canales 8-15)
  sendAtCmdHw("AT+CHE=2");
  sendAtCmdHw("AT+DR=0");       // Data Rate 0 (SF10 125kHz para máximo alcance)
  sendAtCmdHw("AT+TXP=0");      // Potencia de transmisión máxima

  snprintf(cmdBuf, sizeof(cmdBuf), "AT+DEVEUI=%s", devEuiStr);
  sendAtCmdHw(cmdBuf);

  snprintf(cmdBuf, sizeof(cmdBuf), "AT+APPEUI=%s", appEuiStr);
  sendAtCmdHw(cmdBuf);

  snprintf(cmdBuf, sizeof(cmdBuf), "AT+APPKEY=%s", appKeyStr);
  sendAtCmdHw(cmdBuf);
}

void setup() {
  volatile void* dummy1 = (void*)&ADC1_2_IRQHandler;
  volatile void* dummy2 = (void*)&HardFault_Handler;
  (void)dummy1; (void)dummy2;

  pinMode(STATUS_LED_PIN, OUTPUT);
  digitalWrite(STATUS_LED_PIN, HIGH);

  // Inicializar módem RAK3172 por HardwareSerial1 (PB6 TX / PB7 RX)
  configureRak3172Hw();
}

void loop() {
  // Bucle de Join OTAA continuo
  if (!g_loraJoined) {
    g_joinAttempts++;

    digitalWrite(STATUS_LED_PIN, LOW);  delay(100);
    digitalWrite(STATUS_LED_PIN, HIGH); delay(100);

    sendAtCmdHw("AT+JOIN=1:0:10:8");

    unsigned long start = millis();
    while (millis() - start < 10000) {
      if (Serial1.available()) {
        String rx = Serial1.readStringUntil('\n');
        if (rx.indexOf("+EVT:JOINED") != -1 || rx.indexOf("JOINED") != -1) {
          g_loraJoined = 1;
          digitalWrite(STATUS_LED_PIN, LOW); // Encendido fijo al conectar
          break;
        }
      }
    }
  } else {
    // Lectura periódica de medidor óptico cada 30 segundos
    static unsigned long lastRead = 0;
    if (millis() - lastRead > 30000 || lastRead == 0) {
      lastRead = millis();
      MiddeDTS27Reader reader(PA10, PA9);
      MeterData mData;
      if (reader.readMeter(mData, 3000)) {
        g_meterReadValid = 1;
        g_energiaActiva = mData.energiaActivaImp;

        char sendCmd[64];
        snprintf(sendCmd, sizeof(sendCmd), "AT+SEND=10:%08X", (unsigned int)g_energiaActiva);
        sendAtCmdHw(sendCmd);
      } else {
        g_meterReadValid = 0;
      }
    }
  }
}
