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
                +----------------------+----------------------+
                │                      │                      │
+-------------------------------+ +--------------------+ +-------------------------------+
|      MiddeDTS27Reader         | |  ElsterA150Reader  | |      MiddeDDS26DReader        |
| (Trifásico / IEC 62056-21 C)  | | (Monofásico / 8N1) | | (Monofásico / IEC 62056-21 1) |
+-------------------------------+ +--------------------+ +-------------------------------+
                │                      │                      │
                +----------------------+----------------------+
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

## 2. 📋 Medidores Implementados en el Sistema

| Medidor | Tipo | Driver | ID Modelo | Protocolo Óptico | Modo de Operación |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **MIDDE DTS27** | Trifásico | `MiddeDTS27Reader` | `METER_MODEL_MIDDE_DTS27` (2) | IEC 62056-21 Modo C (300 baud 7E1) | Sign-on `/?!\r\n` + ACK `\x06000\r\n` + Volcado OBIS |
| **MIDDE DDS26D** | Monofásico | `MiddeDDS26DReader` | `METER_MODEL_MIDDE_DDS26D` (3) | IEC 62056-21 Modo 1 (2400 baud 7E1) | Sign-on + ACK `\x06031\r\n` + Polling R1 `1.8.0()`, `32.7.0()` |
| **Elster A150** | Monofásico | `ElsterA150Reader` | `METER_MODEL_ELSTER_A150` (1) | Ráfaga Espontánea (2400 baud 8N1) | Captura continua y sincronismo atómico de 186 bytes |

---

## 3. ➕ Guía Paso a Paso para Agregar un Nuevo Medidor

### Paso 1: Crear la Clase Lectora
Crear los archivos `src/NuevoMedidorReader.h` y `src/NuevoMedidorReader.cpp` heredando de `IMeterReader`:
```cpp
#include "IMeterReader.h"

class NuevoMedidorReader : public IMeterReader {
public:
  NuevoMedidorReader(uint8_t rxPin, uint8_t txPin);
  void begin(unsigned long baudRate = 0) override;
  bool readMeter(MeterData &data, unsigned long timeoutMs = 12000) override;
  const char* getMeterName() const override { return "Nuevo Medidor"; }

private:
  uint8_t _rxPin, _txPin;
};
```

### Paso 2: Registrar el Modelo en [`MeterConfig.h`](file:///c:/Users/nahuel/Documents/Antigravity/ElectroKaptor/firmware/src/MeterConfig.h)
```c
#define METER_MODEL_ELSTER_A150   1
#define METER_MODEL_MIDDE_DTS27   2
#define METER_MODEL_MIDDE_DDS26D  3
#define METER_MODEL_NUEVO_MODELO  4

#define SELECTED_METER_MODEL      METER_MODEL_NUEVO_MODELO
```

### Paso 3: Instanciar en [`main.cpp`](file:///c:/Users/nahuel/Documents/Antigravity/ElectroKaptor/firmware/src/main.cpp)
```cpp
#if SELECTED_METER_MODEL == METER_MODEL_MIDDE_DTS27
static MiddeDTS27Reader s_meterReader(IR_RX_PIN, IR_TX_PIN);
#elif SELECTED_METER_MODEL == METER_MODEL_ELSTER_A150
static ElsterA150Reader s_meterReader(IR_RX_PIN, IR_TX_PIN);
#elif SELECTED_METER_MODEL == METER_MODEL_MIDDE_DDS26D
static MiddeDDS26DReader s_meterReader(IR_RX_PIN, IR_TX_PIN);
#elif SELECTED_METER_MODEL == METER_MODEL_NUEVO_MODELO
static NuevoMedidorReader s_meterReader(IR_RX_PIN, IR_TX_PIN);
#endif

static IMeterReader &g_reader = s_meterReader;
```

---

## 4. 📦 Empaquetado Binario Universal (`BitPacker`) y TTN
- Independientemente del medidor físico conectado, el empaquetador [`BitPacker.cpp`](file:///c:/Users/nahuel/Documents/Antigravity/ElectroKaptor/firmware/src/BitPacker.cpp) genera tramas binarias estándar de 15 bytes.
- El decodificador JavaScript en The Things Network (*Payload Formatter*) analiza el campo `tipo_medidor` para renderizar adecuadamente las magnitudes monofásicas o trifásicas.
