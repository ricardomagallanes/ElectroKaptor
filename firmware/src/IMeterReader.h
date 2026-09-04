#ifndef I_METER_READER_H
#define I_METER_READER_H

#include <Arduino.h>

// Estructura unificada que almacena los parámetros leídos de cualquier medidor
struct MeterData {
  bool lecturaValida;
  uint8_t estado;         // 0: Normal, 1: Sin Energia, 2: Sin Lectura, 3: Sin Energia y Sin Lectura
  uint8_t tipoMedidor;    // 0: Monofasico Uni, 1: Monofasico Bi, 2: Trifasico, 3: Grandes Clientes
  uint8_t bateria;        // 0-63 %
  uint8_t cosphi;         // Factor de potencia * 100 (0-100)
  uint16_t voltajeA;      // Volts
  uint16_t voltajeB;      // Volts
  uint16_t voltajeC;      // Volts
  uint16_t corrienteA;    // Amperes * 100
  uint16_t corrienteB;    // Amperes * 100
  uint16_t corrienteC;    // Amperes * 100
  uint32_t energiaActivaImp;  // kWh * 100
  uint32_t energiaActivaExp;  // kWh * 100
  uint32_t energiaReactivaImp; // kVARh * 100
  uint32_t energiaReactivaExp; // kVARh * 100
  uint32_t maximaDemandaImp;   // kW * 100
  uint32_t maximaDemandaExp;   // kW * 100
  uint32_t maximaDemandaImpT1; // kW * 100
  uint32_t maximaDemandaImpT2; // kW * 100
  uint32_t maximaDemandaImpT3; // kW * 100
  uint32_t acumuladaActivaImpT1; // kWh * 100
  uint32_t acumuladaActivaImpT2; // kWh * 100
  uint32_t acumuladaActivaImpT3; // kWh * 100
  uint32_t acumuladaReactivaImpT1; // kVARh * 100
  uint32_t activaImpFase1;     // kWh * 100
  uint32_t activaImpFase2;     // kWh * 100
  uint32_t activaImpFase3;     // kWh * 100
  uint32_t activaExpFase1;     // kWh * 100
  uint32_t activaExpFase2;     // kWh * 100
  uint32_t activaExpFase3;     // kWh * 100
  uint8_t cosphiMinimo;        // * 100
  uint8_t cosphiPromedio;      // * 100
  uint16_t frecuenciaMin;      // Hz * 100
  uint16_t frecuenciaMax;      // Hz * 100
  uint8_t temperatura;         // °C
};

#include "BoardConfig.h"

// Interfaz base abstracta para lectores de medidores de energía
class IMeterReader {
public:
  virtual ~IMeterReader() {}
  virtual void begin(unsigned long baudRate = 2400) = 0;
  virtual bool readMeter(MeterData &data, unsigned long timeoutMs = 10000) = 0;
  virtual const char* getMeterName() const = 0;
};

// Clase base unificada que implementa la orquestación y señalización de LEDs
class BaseMeterReader : public IMeterReader {
public:
  BaseMeterReader(uint8_t rxPin, uint8_t txPin) 
    : _rxPin(rxPin), _txPin(txPin), _lastLedToggle(0), _ledState(false) {}
  
  virtual ~BaseMeterReader() {}

  // Lógica principal unificada con Template Method (MANDATORIA Y CENTRALIZADA)
  bool readMeter(MeterData &data, unsigned long timeoutMs = 10000) override {
    memset(&data, 0, sizeof(MeterData));
    data.lecturaValida = false;
    data.estado = 0;
    _lastLedToggle = 0;
    _ledState = false;

    // Ejecuta la lectura óptica específica del protocolo del medidor
    bool success = performOpticalRead(data, timeoutMs);

    if (success && (data.voltajeA > 0 || data.energiaActivaImp > 0 || data.lecturaValida)) {
      data.lecturaValida = true;
      data.estado = 0;
      notifyOpticalSuccess();
      return true;
    } else {
      data.lecturaValida = false;
      data.estado = 2; // Sin Lectura
      notifyOpticalError();
      return false;
    }
  }

protected:
  uint8_t _rxPin;
  uint8_t _txPin;
  unsigned long _lastLedToggle;
  bool _ledState;

  // Método que cada driver de medidor concreto implementa
  virtual bool performOpticalRead(MeterData &data, unsigned long timeoutMs) = 0;

  // Señalización periódica de actividad óptica en LED 2 (~100ms)
  inline void notifyOpticalActivity() {
    if (millis() - _lastLedToggle > 100) {
      _lastLedToggle = millis();
      _ledState = !_ledState;
      digitalWrite(LED_2_PIN, _ledState ? LOW : HIGH); // Active-LOW
    }
  }

  // Ráfaga rápida en LED 2 (PB1): 8 destellos a 40ms al completar lectura con éxito
  void notifyOpticalSuccess() {
    digitalWrite(LED_2_PIN, HIGH);
    delay(50);
    for (int i = 0; i < 8; i++) {
      digitalWrite(LED_2_PIN, LOW);
      delay(40);
      digitalWrite(LED_2_PIN, HIGH);
      delay(40);
    }
  }

  // Señalización de error: Apagar LED 2 y parpadear LED 3 (PB0) con 4 destellos a 100ms
  void notifyOpticalError() {
    digitalWrite(LED_2_PIN, HIGH);
    for (int i = 0; i < 4; i++) {
      digitalWrite(LED_3_PIN, LOW);
      delay(100);
      digitalWrite(LED_3_PIN, HIGH);
      delay(100);
    }
  }
};

#endif // I_METER_READER_H
