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
2. **Orden y Función Verificada de los LEDs (Lado inferior PCB - Lógica Active-LOW):**
   - **1. LED Verde (D6 - Verde Superior):** Estado de energía de red 220V AC. Conectado por hardware a la fuente Hi-Link; **no se maneja por software**.
   - **2. LED Rojo 2 (D5 - Rojo Superior, Pin `PB1`):** Corresponde al estado **FAIL**. Manejado por software en lógica Active-LOW (`LOW` = Encendido / `HIGH` = Apagado). Parpadea ante cualquier fallo/alerta de comunicación y luego se apaga.
   - **3. LED Rojo 3 (D3 - Rojo Medio, Pin `PB0`):** Corresponde a la función **LoRaWAN / IR**. Manejado por software en lógica Active-LOW (`LOW` = Encendido / `HIGH` = Apagado). Parpadea durante la solicitud de Join OTAA y se mantiene encendido fijo durante la transmisión del dato por LoRaWAN; luego se apaga.
   - **4. LED Rojo 4 (D4 - Rojo Inferior, Pin `PB2`):** Reserva / No se usa. Manejado por software en lógica Active-LOW (`LOW` = Encendido / `HIGH` = Apagado).
3. **Conectores Serie y Headers:**
   - **Header SWD:** 4 pads en la parte superior para programación ST-Link V2 (`SWDIO`, `SWCLK`, `3.3V`, `GND`).
   - **Header J12 y Resistencias R20 / R21:** Forman el bus de comunicación serie UART de comandos AT entre el STM32F103 (Host) y los pines `UART2_TX` / `UART2_RX` del módulo RAK3172. `R20` y `R21` son resistencias en serie de protección en las líneas TX/RX.
   - **Header J13 (4 pines a la izquierda del RAK3172):** Conector directo a la interfaz de depuración/programación SWD propia del chip STM32WLE5CC interno del RAK3172.
   - **Pulsadores:** `SW3` (conectado a la línea de Reset `NRST` del RAK3172) y `SW2` (Reset/Configuración general).

### 🔍 Detalle del Conexionado Físico y Hardware Verificado (Ensayo Exitoso):

A partir del ensayo de comunicación serie validado en placa, la distribución oficial y probada de conexiones entre el módem LoRaWAN **RAK3172** y el MCU **STM32F103C8T6** es la siguiente:

| Pin RAK3172(H) / Periférico | Señal | Pin STM32F103 (LQFP48) | Señal STM32 | Periférico Hardware STM32 | Función / Descripción de la Línea |
| :---: | :--- | :---: | :--- | :--- | :--- |
| **Puerto Óptico TX** | `IR_TX` | **Pin 21** | `PB10` | GPIO Output / `USART3_TX` | Transmisión hacia el LED IR del puerto óptico. |
| **Puerto Óptico RX** | `IR_RX` | **Pin 13** | `PA3` | GPIO Input / `USART2_RX` | Recepción desde el fototransistor del puerto óptico. |
| **Relay Desconexión** | `RELAY_DISC` | **Pin 29** | `PA8` | GPIO Output / `TIM1_CH1` | Control de Relay para desconexión/corte remoto de luz. |
| **Relay Conexión** | `RELAY_CONN` | **Pin 30** | `PA9` | GPIO Output / `USART1_TX` | Control de Relay para reconexión remota de luz. |
| **RAK3172 Pin 1** | `UART2_RX` | **Pin 42** | `PB6` | `USART1_TX` (Remapped) | Transmisión serie Hardware del MCU (TX) hacia el RAK3172. |
| **RAK3172 Pin 2** | `UART2_TX` | **Pin 43** | `PB7` | `USART1_RX` (Remapped) | Recepción serie Hardware del MCU (RX) desde el RAK3172. |
| **RAK3172 Pin 22** | `NRST` | **Pin 45** | `PB8` | GPIO Output (Control NRST) | Control de Reset por Hardware (`NRST`) del RAK3172. |
| **RAK3172 Pin 24** | `VDD` | **Pines 24 y 36** | `VDD_1` / `VDD_2` | 3.3V DC Power Rail | Alimentación principal de 3.3V DC del módem RAK3172. |
| **RAK3172 Pin 21, 23** | `BOOT` / `GND` | **Pines 47, 8, 23, 35** | `VSS_3`, `VSSA`, `VSS_1`, `VSS_2` | Plano de Masa (GND) | Plano de masa / tierra común y modo Boot normal. |

> [!IMPORTANT]
> **CONFIGURACIÓN LORAWAN Y PERIFÉRICO SERIE:**
> * **Periférico:** `USART1` remapeado vía AFIO (`__HAL_AFIO_REMAP_USART1_ENABLE()`).
> * **Velocidad de Baudios:** **115200 8N1** (Baudrate por defecto del firmware RUI3 v4.0.6).
> * **Banda Regional LoRaWAN:** **AU915** (`AT+BAND=6`).
> * **Máscara de Canales:** **Sub-banda 2 / FSB2** (`AT+MASK=0002` / Canales 8 al 15 usados por TTN).
> * **Respuesta de Confirmación de Join OTAA:** `+EVT:JOINED`.

---

## 4. 💡 Código de LEDs y Estados de Diagnóstico (Lógica Active-LOW)

Los 4 LEDs del lado inferior de la PCB `ME_LoRa_v3.6` se manejan en lógica Active-LOW (`LOW` = Encendido / `HIGH` = Apagado):

1. **LED Verde (D6):** Alimentación 220V AC (Físico por Hi-Link, no controlado por software).
2. **LED Rojo 3 (`PB0` / D3 - Medio):** **LED LoRaWAN**. Parpadea durante la negociación de Join OTAA y se enciende fijo durante la transmisión por radio de la telemetría.
3. **LED Rojo 2 (`PB1` / D5 - Superior):** **LED ERROR**. Parpadea ante fallos de lectura del medidor o error en la transmisión LoRaWAN.
4. **LED Rojo 4 (`PB2` / D4 - Inferior):** **LED Reserva**.
5. **LED Onboard (`PC13`):** **Heartbeat MCU**. Parpadea brevemente en cada ciclo de ejecución del bucle principal.

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
