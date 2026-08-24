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

// Interfaz base abstracta para lectores de medidores de energía
class IMeterReader {
public:
  virtual ~IMeterReader() {}
  virtual void begin(unsigned long baudRate = 2400) = 0;
  virtual bool readMeter(MeterData &data, unsigned long timeoutMs = 10000) = 0;
  virtual const char* getMeterName() const = 0;
};

#endif // I_METER_READER_H
