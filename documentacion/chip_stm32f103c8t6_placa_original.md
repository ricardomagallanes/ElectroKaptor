# 🔍 Diagnóstico y Especificaciones Técnicas - Placa Original (ME_LoRa_v3.6)

Este documento contiene la radiografía técnica completa y el relevamiento de hardware de la placa de telemetría original **ME_LoRa_v3.6**.

---

## 1. ⚙️ Especificaciones del Microcontrolador Anfitrión (Host MCU)

| Parámetro | Detalle Técnico |
| :--- | :--- |
| **Chip Integrado (U5)** | `STM32F103C8T6` |
| **Fabricante** | STMicroelectronics |
| **Arquitectura de Núcleo** | ARM® 32-bit Cortex®-M3 RISC |
| **Frecuencia Máxima de Reloj** | 72 MHz (mediante PLL con cristal de 16 MHz) |
| **Memoria Flash** | 64 KB (128 KB direccionables) |
| **Memoria SRAM** | 20 KB |
| **Encapsulado** | LQFP48 (48 pines de plástico) |
| **Cristal Oscilador (Y1)** | 16.000 MHz (marcado `YXC 16.000 536M21`) |
| **Voltaje de Operación** | 3.3V DC |

---

## 2. 📡 Módem LoRaWAN Integrado

| Parámetro | Detalle Técnico |
| :--- | :--- |
| **Módulo Radio (U8)** | **RAK3172 (I)** (RAKwireless) |
| **Chip Base del Módem** | STM32WLE5CC (Semtech SX1262 LoRa Core) |
| **DevEUI de Fábrica** | `<DEVEUI_PLACA_ORIGINAL>` |
| **ID Homologaciones** | FCC ID: `2AF6B-RAK3172` | IC: `25908-RAK3172` |
| **Conector de Antena** | U.FL / IPEX |
| **Protocolo de Control** | Comandos AT por UART serie (por defecto `9600` u `115200` baudios 8N1) |

---

## 3. 🧩 Relevamiento de Componentes en la PCB `ME_LoRa_v3.6`

```
  +-------------------------------------------------------------------+
  |               Placa ME_LoRa_v3.6 (Placa Original)                 |
  |                                                                   |
  |  [Hi-Link HLK-PM01]        [Pines SWD Header]                     |
  |  (Fuente 220V->5V)         (VCC / SWDIO / SWCLK / GND)            |
  |                                 |                                 |
  |  [Borneras Verdes] <---->  STM32F103C8T6 (U5) ---> [4x LEDs]      |
  |  (Entrada AC / Optica)       | (ARM Cortex-M3)    D6 (Verde - R19)  |
  |                              |                    D5 (Rojo - R18)   |
  |  [Módem RAK3172] <------- (USART)                 D3 (Rojo - R16)   |
  |  (RAK3172 LoRaWAN)                                  D4 (Rojo - R17)   |
  +-------------------------------------------------------------------+
```

### Componentes Identificados:
1. **Fuente Interna AC/DC:** Módulo **Hi-Link HLK-PM01** (Entrada 100-240V AC -> Salida 5V DC 3W), protegido por fusible de 500mA (`F1`) y varistor de protección frente a sobretensiones (`KV1`).
2. **LEDs Indicadores de Estado (Lado inferior PCB):**
   - **D6 (Verde superior):** Indicador de Alimentación de Red 220V AC por Hardware (Puenteado en PCB directamente a la salida de la fuente Hi-Link; no depende de control GPIO por firmware).
   - **D5 (Rojo superior):** Pin `PB13` (Control por Software - Falla / Alerta).
   - **D3 (Rojo medio):** Pin `PB14` (Control por Software - Actividad Lectura IR / LoRaWAN).
   - **D4 (Rojo inferior):** Pin `PB15` (Control por Software - Estado Auxiliar).
3. **Conectores Serie y Headers:**
   - **Header SWD:** 4 pads en la parte superior para programación ST-Link V2 (`SWDIO`, `SWCLK`, `3.3V`, `GND`).
   - **Header J12 y Resistencias R20 / R21:** Forman el bus de comunicación serie UART de comandos AT entre el STM32F103 (Host) y los pines `UART2_TX` / `UART2_RX` del módulo RAK3172. `R20` y `R21` son resistencias en serie de protección en las líneas TX/RX.
   - **Header J13 (4 pines a la izquierda del RAK3172):** Conector directo a la interfaz de depuración/programación SWD propia del chip STM32WLE5CC interno del RAK3172.
   - **Pulsadores:** `SW3` (conectado a la línea de Reset `NRST` del RAK3172) y `SW2` (Reset/Configuración general).

### 🔍 Detalle del Conexionado del RAK3172 con el STM32F103:
- **Comunicación:** El RAK3172 opera como un módem esclavo mediante comandos AT a 9600 u 115200 baudios.
- **Pines del RAK3172 involucrados:**
  - Pin 5 (`UART2_TX` de RAK3172) ---> pasa por resistencia `R20` ---> conecta al pin `RX` de la UART del STM32F103 (`PA10` en USART1 o `PA3` en USART2).
  - Pin 6 (`UART2_RX` de RAK3172) ---> pasa por resistencia `R21` ---> conecta al pin `TX` de la UART del STM32F103 (`PA9` en USART1 o `PA2` en USART2).
  - Pin 7 (`NRST` del RAK3172) ---> conectado al pulsador `SW3` y a un GPIO de control del STM32F103 para reinicio por software.

---

## 4. 🛡️ Procedimiento de Desbloqueo y Lectura RDP (Readout Protection)

El chip **STM32F103C8T6** viene de fábrica con **RDP Nivel 1** activado por el fabricante para evitar la extracción directa del binario.

### Características del Desbloqueo:
- Al intentar leer el chip por el puerto SWD, devolverá error de lectura.
- Al ejecutar el comando de **Read Out Unprotect**, el chip ejecutará un **Mass Erase (borrado masivo)** completo de la Flash por hardware.
- **El microcontrolador NO se invalida ni se daña.** Queda completamente en Nivel 0 (abierto) para recibir el firmware **ElectroKaptor** compilado desde PlatformIO.

---

## 5. 🛠️ Conexión del ST-Link V2 a la Placa `ME_LoRa_v3.6`

Para programar esta placa con el ST-Link V2:

| ST-Link V2 | Pin STM32F103C8T6 (LQFP48) | Ubicación en PCB |
| :--- | :--- | :--- |
| **SWDIO** | **Pin 34 (`PA13`)** | Pad del Header SWD superior |
| **SWCLK** | **Pin 37 (`PA14`)** | Pad del Header SWD superior |
| **GND** | **GND (Tierra)** | Plano GND de la placa |
| **3.3V** | **3.3V (VCC)** | Pin 3.3V del regulador U2 |

---

## 6. 💻 Comandos de Consola para Flashear la Placa Original

### A. Desbloquear y Borrar Chip por ST-Link:
```bash
STM32_Programmer_CLI.exe -c port=SWD mode=UR -readunprotect
```

### B. Compilar y Subir Firmware con PlatformIO:
```bash
python -m platformio run -e stm32f103c8_original_board -t upload
```
