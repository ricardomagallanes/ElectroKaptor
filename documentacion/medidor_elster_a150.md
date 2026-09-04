# ⚡ Especificación Técnica de Integración: Medidor Monofásico Elster A150
## Driver de Lectura Óptica Infrarroja y Mapeo de Telemetría

---

## 1. 📖 Descripción General del Medidor
El **Elster A150** es un medidor electrónico de energía eléctrica monofásico para pequeñas demandas residenciales y comerciales.
- **Tipo de Conexión:** Monofásico (Fase y Neutro / $V_A$, $I_A$).
- **Puerto de Comunicación:** Puerto óptico infrarrojo frontal (estándar FLAG / IEC 62056-21 mecánico).
- **Modo de Transmisión Óptica:** **Unidireccional / Ráfaga Espontánea Continua** (Broadcast periódico sin requerir comando de interrogación o handshake previo).

---

## 2. 📡 Especificación de la Capa Física y Enlace Serie
- **Velocidad de Transmisión:** **2400 baudios**
- **Formato de Carácter:** **8N1** (8 bits de datos, sin paridad, 1 bit de parada).
- **Tiempo de Bit nominal ($T_{bit}$):**
  $$\text{Bit Time} = \frac{1\,000\,000\,\mu\text{s}}{2400} = 416.67\,\mu\text{s} \approx 417\,\mu\text{s}$$
- **Tiempo de Medio Bit:** $208\,\mu\text{s}$ (utilizado para muestreo centrado anti-ruido).
- **Lógica de Señal:** 
  - **Reposo (MARK / IDLE):** Nivel `HIGH` (3.3V en `PA3` con `INPUT_PULLUP`).
  - **Bit de Inicio (SPACE / START):** Flanco descendente `HIGH -> LOW`.
  - **Bits de Datos:** LSB primero.
  - **Bit de Parada (STOP):** Nivel `HIGH`.

---

## 3. 🧩 Estructura de la Trama Óptica (186 Bytes)

El medidor emite ráfagas periódicas de exactamente **186 bytes** con la siguiente estructura:

```
+------------------------------------+--------------------------------------------------+
|   Cabecera de Sincronismo (8 B)    |           Cuerpo de Datos Útiles (178 B)         |
| [ 57 55 55 55 7F 77 5D 55 ]        | [ Bytes 8 a 185: Registros modulados 4-a-8 bits] |
+------------------------------------+--------------------------------------------------+
```

### 3.1. Secuencia de Sincronismo (Header)
Los primeros 8 bytes corresponden al patrón identificador único de trama:
```c
const uint8_t syncPattern[8] = { 0x57, 0x55, 0x55, 0x55, 0x7F, 0x77, 0x5D, 0x55 };
```

---

## 4. 🔄 Algoritmo de Decodificación y Desmodulación

La carga útil de datos (desde el byte 8 al 185) requiere dos transformaciones matemáticas consecutivas:

### Paso 1: Permutación / Swap de Bytes (Alto / Bajo)
A partir del índice 8, los bytes se transmiten con el orden invertido por pares:
```c
for (uint8_t i = 8; i < 186; i += 2) {
    bytesOrdenados[i]     = bytesRecibidos[i + 1];
    bytesOrdenados[i + 1] = bytesRecibidos[i];
}
```

### Paso 2: Desmodulación 4-a-8 Bits a Nibbles BCD (0x0 a 0xF)
Cada byte recibido representa un nibble BCD según la siguiente tabla de mapeo de lógica positiva:

| Byte Recibido (Hex) | Byte Recibido (Dec) | Nibble Decodificado (Hex) |
| :---: | :---: | :---: |
| `0x55` | 85 | `0x0` |
| `0x57` | 87 | `0x1` |
| `0x5D` | 93 | `0x2` |
| `0x5F` | 95 | `0x3` |
| `0x75` | 117 | `0x4` |
| `0x77` | 119 | `0x5` |
| `0x7D` | 125 | `0x6` |
| `0x7F` | 127 | `0x7` |
| `0xD5` | 213 | `0x8` |
| `0xD7` | 215 | `0x9` |
| `0xDD` | 221 | `0xA` |
| `0xDF` | 223 | `0xB` |
| `0xF5` | 245 | `0xC` |
| `0xF7` | 247 | `0xD` |
| `0xFD` | 253 | `0xE` |
| `0xFF` | 255 | `0xF` |

---

## 5. 🗺️ Mapa de Extracción de Registros Eléctricos

Una vez obtenidos los 186 nibbles desmodulados, los valores físicos se extraen mediante ponderación posicional decimal:

### 1. Voltaje Fase A ($V_A$)
- **Índices de Nibbles:** `132` a `130` (3 dígitos decimales).
- **Cálculo:**
  $$V_A = \text{nibble}[132] \times 100 + \text{nibble}[131] \times 10 + \text{nibble}[130]$$
- **Unidad:** Voltios enteros (ej: $230\text{ V}$).
- **Fases B y C:** Se asignan a `0` (monofásico).

### 2. Corriente Fase A ($I_A$)
- **Índices de Nibbles:** `125` a `123` (3 dígitos).
- **Cálculo:**
  $$\text{val} = \text{nibble}[125] \times 100 + \text{nibble}[124] \times 10 + \text{nibble}[123]$$
  $$I_A = (\text{val} \gg 2) \times 10 \quad (\text{en centésimas de Amperio, } A \times 100)$$
- **Fases B y C:** Se asignan a `0`.

### 3. Energía Activa Importada ($E_{activa}$)
- **Índices de Nibbles:** `69` a `62` (8 dígitos decimales).
- **Cálculo:** Ponderación BCD de $10^7$ a $10^0$.
- **Unidad:** Centésimas de kWh ($kWh \times 100$).

### 4. Demanda Máxima Activa Importada ($P_{max}$)
- **Índices de Nibbles:** `179` a `177` (3 dígitos decimales).
- **Cálculo:** $\text{val} \times 10$ ($kW \times 100$).

### 5. Factor de Potencia ($\cos\varphi$)
- **Índices de Nibbles:** `116` a `115` (2 dígitos decimales).
- **Unidad:** Centésimas de factor de potencia ($0\text{ a }100 \rightarrow 0.00\text{ a }1.00$).

### 6. Energía Reactiva Importada ($Q_{imp}$)
- **Índices de Nibbles:** `80` a `77` (4 dígitos decimales).
- **Cálculo:** $\text{val} \ll 10$ ($kVARh \times 100$).

---

## 6. 🏗️ Integración en la Arquitectura de Firmware

El driver está implementado en la clase [`ElsterA150Reader`](file:///c:/Users/nahuel/Documents/Antigravity/ElectroKaptor/firmware/src/ElsterA150Reader.h), cumpliendo la interfaz polimórfica [`IMeterReader`](file:///c:/Users/nahuel/Documents/Antigravity/ElectroKaptor/firmware/src/IMeterReader.h):

```cpp
#include "IMeterReader.h"

class ElsterA150Reader : public IMeterReader {
public:
  ElsterA150Reader(uint8_t rxPin, uint8_t txPin);
  void begin(unsigned long baudRate = 2400) override;
  bool readMeter(MeterData &data, unsigned long timeoutMs = 12000) override;
  const char* getMeterName() const override { return "Elster A150 (Monofasico)"; }
};
```

### Activación en Configuración
En `firmware/src/MeterConfig.h`:
```c
#define SELECTED_METER_MODEL METER_MODEL_ELSTER_A150
```

---

## 7. 🔒 Requisitos Críticos de Temporización Hardware (Zero-Jitter Bitbang)

Para evitar la pérdida de sincronismo en la ráfaga continua de 186 bytes a 2400 baudios:
1. **Deshabilitación de Interrupciones en Muestreo:** Durante el muestreo atómico de los 8 bits de cada byte, se deshabilitan las interrupciones del microcontrolador (`noInterrupts()` / `interrupts()`) para evitar que el timer `SysTick` de Cortex-M3 corra el punto de lectura.
2. **Cero Salidas Serie Intermedias:** Durante la captura de los 178 bytes del payload, no se deben emitir prints de debug que bloqueen el CPU por UART.
3. **Muestreo Centrado:** El flanco de inicio se detecta en el 50% del bit ($208\,\mu\text{s}$) y cada bit sucesivo se muestrea con un retraso exacto de $417\,\mu\text{s}$.
