# ElectroKaptor ⚡📟

**ElectroKaptor** es un sistema de telemetría y lectura remota modular para medidores de energía eléctrica (**Elster A150**, **Midde Trifásico DTS27** y futuros modelos), basado en el microcontrolador **Heltec ESP32-S3 (WiFi LoRa 32 V3)** con comunicación **LoRaWAN (TTN)** y sonda infrarroja (IR).

## 🚀 Características

- **Soporte Multi-Medidor Modular:** Arquitectura C++ orientada a objetos (`IMeterReader`, `MeterReaderFactory`) que permite seleccionar el modelo de medidor mediante un archivo de configuración (`MeterConfig.h`).
- **Medidores Soportados:**
  - **Elster A150** (Monofásico / Grandes clientes).
  - **MIDDE Trifásico DTS27** (Trifásico).
- **Lectura por Sonda Óptica IR (IEC 62056-21):** Comunicación directa vía puerto serie infrarrojo a 2400 baudios.
- **Empaquetamiento Eficiente de Datos:** Comprime y empaqueta en bits 33 parámetros eléctricos (Energía Activa/Reactiva Import/Export, Voltaje, Corriente, Factor de Potencia, Máxima Demanda, Diagnósticos, etc.) divididos en 4 tipos de mensajes compactos (Mensajes 0 a 3).
- **Conectividad LoRaWAN (OTAA):** Transmisión de tramas empaquetadas a través de The Things Network (TTN).
- **Decodificador JS para TTN (LNS v3):** Incluye payload formatter con detección y normalización de payloads tanto binarios crudos como cadenas ASCII Hexadecimales.

## ⚙️ Configuración del Medidor Activo

Para seleccionar el modelo de medidor a compilar en el firmware, edita el archivo [`firmware/src/MeterConfig.h`](file:///c:/Users/nahuel/Documents/Antigravity/PayloadDecoder/firmware/src/MeterConfig.h):

```cpp
// Para medir MIDDE Trifásico DTS27:
#define SELECTED_METER_MODEL METER_MODEL_MIDDE_DTS27

// Para medir Elster A150:
// #define SELECTED_METER_MODEL METER_MODEL_ELSTER_A150
```

## 📁 Estructura del Repositorio

- [`firmware/`](file:///c:/Users/nahuel/Documents/Antigravity/PayloadDecoder/firmware/): Código fuente en C++/Arduino para PlatformIO y Arduino IDE (ESP32-S3).
  - `MeterConfig.h`: Configuración central del modelo de medidor activo.
  - `IMeterReader.h`: Interfaz base para lectores de medidores.
  - `ElsterA150Reader.h / .cpp`: Lector para medidores Elster A150.
  - `MiddeDTS27Reader.h / .cpp`: Lector para medidores MIDDE Trifásico DTS27.
  - `MeterReaderFactory.h`: Fábrica para instanciación dinámica del lector activo.
- [`ttn_decoder/`](file:///c:/Users/nahuel/Documents/Antigravity/PayloadDecoder/ttn_decoder/): Decodificador en Javascript (`decoder.js`) e instructivo de configuración para The Things Network (`README_TTN.md`).
- [`docs/`](file:///c:/Users/nahuel/Documents/Antigravity/PayloadDecoder/docs/): Esquemas de hardware y guía de conexión de la sonda óptico-infrarroja (`diagrama_conexion.md`).
- [`documentacion/`](file:///c:/Users/nahuel/Documents/Antigravity/PayloadDecoder/documentacion/): Documentos técnicos en PDF.
- [`parametros.txt`](file:///c:/Users/nahuel/Documents/Antigravity/PayloadDecoder/parametros.txt): Lista detallada de los 33 parámetros leídos y empaquetados.

## 🛠️ Hardware Requerido

1. **Microcontrolador:** Heltec ESP32-S3 (WiFi LoRa 32 V3) - Chip SX1262.
2. **Sonda Infrarroja IR:**
   - Opción A: Sonda comercial IEC 62056-21 con salida TTL (TX/RX).
   - Opción B: Circuito fototransistor/LED IR casero (ver [`docs/diagrama_conexion.md`](file:///c:/Users/nahuel/Documents/Antigravity/PayloadDecoder/docs/diagrama_conexion.md)).
3. **Medidor:** Elster A150 / Midde Trifásico DTS27.

## 🔌 Conexiones de Hardware

| Pin ESP32-S3 | Conexión Sonda IR |
| :--- | :--- |
| **GPIO 18** | RX (Receptor IR) |
| **GPIO 17** | TX (Emisor IR) |
| **3V3** | VCC |
| **GND** | GND |

## 📜 Licencia

Proyecto de telemetría y monitoreo de medidores eléctricos.
