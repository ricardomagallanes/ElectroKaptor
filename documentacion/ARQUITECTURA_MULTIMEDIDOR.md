# 🏗️ Guía de Arquitectura Multimedidor y Adición de Nuevos Equipos
## Sistema ElectroKaptor (STM32 / ESP32)

---

## 1. 🎯 Principio de Diseño: Aislamiento y Polimorfismo
Para permitir la integración de múltiples modelos de medidores (monofásicos, trifásicos, diferentes protocolos y fabricantes) **sin alterar el código existente ni introducir regresiones**, el sistema utiliza una arquitectura basada en **Interfaces Polimórficas** y **Estructuras de Datos Unificadas**.

```
                           +------------------------+
                           |  IMeterReader (Base)   |
                           +------------------------+
                           | + begin()              |
                           | + readMeter(MeterData) |
                           | + getMeterName()       |
                           +------------------------+
                                       ▲
                                       │ (Herencia / Polimorfismo)
                 +---------------------+---------------------+
                 │                                           │
+----------------------------------+       +----------------------------------+
|      MiddeDTS27Reader            |       |       ElsterA150Reader           |
| (Trifásico / IEC 62056-21 Modo C)|       | (Monofásico / 2400 baudios 8N1)  |
+----------------------------------+       +----------------------------------+
                 │                                           │
                 +---------------------+---------------------+
                                       │
                                       ▼
                           +------------------------+
                           |  MeterData (Unificado) |
                           +------------------------+
                                       │
                                       ▼
                           +------------------------+
                           |   BitPacker (Binario)  |
                           +------------------------+
                                       │
                                       ▼
                           +------------------------+
                           |    LoRaWAN (RAK3172)   |
                           +------------------------+
```

---

## 2. 📁 Estructura de Clases del Subsistema Óptico

### 2.1. Interfaz Base [`IMeterReader.h`](file:///c:/Users/nahuel/Documents/Antigravity/ElectroKaptor/firmware/src/IMeterReader.h)
Define el contrato obligatorio que todo nuevo driver de medidor debe implementar:
```cpp
class IMeterReader {
public:
  virtual ~IMeterReader() {}
  virtual void begin(unsigned long baudRate = 0) = 0;
  virtual bool readMeter(MeterData &data, unsigned long timeoutMs = 12000) = 0;
  virtual const char* getMeterName() const = 0;
};
```

### 2.2. Estructura Unificada de Datos `MeterData`
Centraliza todos los campos eléctricos medidos para ser entregados al empaquetador:
```cpp
struct MeterData {
  bool     lecturaValida;      // true si la lectura óptica fue exitosa
  uint8_t  tipoMedidor;        // 1=Monofásico, 2=Trifásico, 3=Grandes Clientes
  uint8_t  estado;             // 0=Normal, 1=Alerta, 2=Sin Lectura, etc.
  uint8_t  bateria;            // % batería interna (ej: 90)
  uint8_t  temperatura;        // °C MCU/Ambiente
  uint8_t  cosphi;             // Factor de potencia (x100)
  uint16_t frecuenciaMin;      // Frecuencia de red (x100, ej: 5000 = 50.00 Hz)
  uint16_t frecuenciaMax;      // Frecuencia max
  uint16_t voltajeA, voltajeB, voltajeC; // Tensión en Volts
  uint16_t corrienteA, corrienteB, corrienteC; // Corriente en A*100
  uint32_t energiaActivaImp;   // kWh * 100
  uint32_t energiaActivaExp;   // kWh * 100
  uint32_t energiaReactivaImp; // kVARh * 100
  uint32_t energiaReactivaExp; // kVARh * 100
  uint32_t maximaDemandaImp;   // kW * 100
  uint32_t maximaDemandaExp;   // kW * 100
};
```

---

## 3. 🛠️ Paso a Paso: Cómo Agregar un Nuevo Medidor

Para incorporar un nuevo modelo (ej: *Medidor Hexing, Landis+Gyr, Actaris, etc.*):

### Paso 1: Crear la clase del Driver
Crear los archivos `NuevoMedidorReader.h` y `NuevoMedidorReader.cpp` heredando de `IMeterReader`:
```cpp
#include "IMeterReader.h"

class NuevoMedidorReader : public IMeterReader {
public:
  NuevoMedidorReader(uint8_t rxPin, uint8_t txPin);
  void begin(unsigned long baudRate = 0) override;
  bool readMeter(MeterData &data, unsigned long timeoutMs = 10000) override;
  const char* getMeterName() const override { return "Nombre Medidor"; }

private:
  uint8_t _rxPin, _txPin;
};
```

### Paso 2: Registrar el Modelo en [`MeterConfig.h`](file:///c:/Users/nahuel/Documents/Antigravity/ElectroKaptor/firmware/src/MeterConfig.h)
```c
#define METER_MODEL_MIDDE_DTS27   1
#define METER_MODEL_ELSTER_A150   2
#define METER_MODEL_NUEVO_MODELO  3

#define SELECTED_METER_MODEL      METER_MODEL_NUEVO_MODELO
```

### Paso 3: Instanciar en [`main.cpp`](file:///c:/Users/nahuel/Documents/Antigravity/ElectroKaptor/firmware/src/main.cpp)
```cpp
#if SELECTED_METER_MODEL == METER_MODEL_MIDDE_DTS27
static MiddeDTS27Reader s_meterReader(IR_RX_PIN, IR_TX_PIN);
#elif SELECTED_METER_MODEL == METER_MODEL_ELSTER_A150
static ElsterA150Reader s_meterReader(IR_RX_PIN, IR_TX_PIN);
#elif SELECTED_METER_MODEL == METER_MODEL_NUEVO_MODELO
static NuevoMedidorReader s_meterReader(IR_RX_PIN, IR_TX_PIN);
#endif

static IMeterReader &g_reader = s_meterReader;
```

---

## 4. 📦 Empaquetado Binario Universal (`BitPacker`) y TTN
- Independientemente del medidor físico conectado, el empaquetador [`BitPacker.cpp`](file:///c:/Users/nahuel/Documents/Antigravity/ElectroKaptor/firmware/src/BitPacker.cpp) genera tramas binarias estándar:
  - **Trama 0 (Mensaje 0):** Telemetría Principal (15 bytes).
  - **Trama 1 (Mensaje 1):** Energías secundarias y demandas máximas (16 bytes).
- El decodificador JavaScript en The Things Network (*Payload Formatter*) analiza el campo `tipo_medidor` para renderizar adecuadamente las magnitudes monofásicas o trifásicas.
