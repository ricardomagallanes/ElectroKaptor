# 🔍 Diagnóstico y Especificaciones Técnicas - Placa Original (STM32F103C8T6 + RAK3172)

Este documento detalla la arquitectura de hardware, especificaciones técnicas y procedimiento de reprogramación para la placa de telemetría original del sistema **ElectroKaptor**.

---

## 1. ⚙️ Especificaciones del Microcontrolador Principal

| Parámetro | Detalle Técnico |
| :--- | :--- |
| **Chip Integrado** | `STM32F103C8T6` |
| **Fabricante** | STMicroelectronics |
| **Arquitectura de Núcleo** | ARM® 32-bit Cortex®-M3 RISC |
| **Frecuencia Máxima de Reloj** | 72 MHz (mediante PLL interno) |
| **Memoria Flash** | 64 KB (ampliable a 128 KB en bancos de densidad media) |
| **Memoria SRAM** | 20 KB |
| **Encapsulado** | LQFP48 (48 pines de plástico) |
| **Cristal Oscilador Externo (HSE)** | 16.000 MHz (marcado `YXC 16.000`) |
| **Voltaje de Operación** | 2.0 V a 3.6 V (típico 3.3V) |
| **Interfaces de Serie (UART/USART)** | 3 puertos independientes (USART1, USART2, USART3) |

---

## 2. 🧩 Arquitectura de Componentes de la Placa Original

La placa original divide la lógica en un procesador anfitrión (**STM32F103C8T6**) y un módem LoRaWAN dedicado (**RAK3172**):

```
                       +-----------------------------------+
                       |    Placa de Telemetría Original   |
                       |                                   |
  [Sonda IR Optica] <---> [USART_x]                 [SWD] <---> [ST-Link V2]
  (300 baud 7E1)       |    STM32F103C8T6           (PA13/PA14)
                       |  (Cortex-M3 Host)                 
  [4x LEDs Estado] <--- |   GPIO Pins               [USART_y] <---> [Módem RAK3172]
  (Alimentación/Tx/Rx)  |                           (AT Cmds)    (LoRaWAN AU915)
                       +-----------------------------------+
```

### Componentes Clave:
1. **Microcontrolador Anfitrión (STM32F103C8T6):** Gestiona la lógica de control, temporizadores, parpadeo de LEDs y el protocolo IEC 62056-21 del puerto óptico IR.
2. **Módem LoRaWAN (RAK3172):** Módulo certificado basado en el chip `STM32WLE5CC`. Se comunica con el STM32F103 mediante comandos AT estándar por puerto serie UART (ej: `AT+JOIN`, `AT+SEND=10:...`).
3. **Sonda Óptica Infrarroja (IR):** Conectada a un puerto UART del STM32F103 mediante drivers transistorizados para comunicarse con medidores de energía (DTS27 / Elster A150).
4. **4 LEDs Indicadores:** Indicación visual de estado (Alimentación, Lectura IR, Transmisión LoRa, Error/Estado).
5. **Puerto de Depuración/Programación SWD:** Pines de 4 hilos (SWDIO, SWCLK, 3.3V, GND).

---

## 3. 🛡️ Mecanismo de Seguridad y Desbloqueo (RDP Level 1)

El `STM32F103C8T6` utiliza el sistema **Readout Protection (RDP)** a través de los **Option Bytes** en la Flash (dirección `0x1FFFF800`):

- **RDP Nivel 0 (Fábrica/Abierto):** Lectura y escritura permitidas sin restricciones por puerto SWD.
- **RDP Nivel 1 (Protegido por fabricante):** El puerto SWD rechaza las lecturas de memoria Flash (*Memory Read Error*).
- **Procedimiento de Desbloqueo:** Si la placa original tiene RDP Nivel 1 activo, al enviar el comando de cambio a RDP Nivel 0 mediante `STM32CubeProgrammer` o `st-flash`, el microcontrolador ejecuta un **Mass Erase (borrado masivo automático)** por hardware. 

> [!NOTE]
> El proceso de desprotección borra el firmware original de fábrica, pero **NO daña ni inutiliza el integrado**. El chip queda completamente limpio y listo para grabar el firmware **ElectroKaptor**.

---

## 4. 🔎 Estrategia para el Mapeo e Interpretación de Pines (Pinout Discovery)

Como los pines exactos conectados a los periféricos deben identificarse, utilizaremos el siguiente flujo metodológico:

### A. Pines Fijos por Hardware en STM32F103C8T6:
- **SWDIO (Depuración):** Pin 34 (`PA13`)
- **SWCLK (Depuración):** Pin 37 (`PA14`)
- **BOOT0:** Pin 44
- **NRST (Reset):** Pin 7
- **Cristal 16MHz (OSC_IN / OSC_OUT):** Pines 5 (`PD0`) y 6 (`PD1`)

### B. Mapeo de Puertos Serie UART Posibles:
El STM32F103C8T6 posee 3 puertos USART por hardware:
- **USART1:** TX = `PA9` (Pin 30) | RX = `PA10` (Pin 31)
- **USART2:** TX = `PA2` (Pin 12) | RX = `PA3` (Pin 13)
- **USART3:** TX = `PB10` (Pin 21) | RX = `PB11` (Pin 22)

*Uno de estos puertos está conectado al RAK3172 (9600 o 115200 baudios) y otro a la sonda IR (300 baudios 7E1).*

### C. Mapeo de los 4 LEDs:
- Medición de continuidad con multímetro desde el ánodo/cátodo de los 4 LEDs hacia los pines del STM32.
- O mediante un firmware de escaneo de GPIOs en bucle (Pin Scanner).

---

## 5. 🛠️ Conexión del Programador ST-Link V2

Para conectarse a la placa original con el programador ST-Link V2:

| Pin ST-Link V2 | Pin en Placa Original / STM32F103 |
| :--- | :--- |
| **SWDIO** | Pin `PA13` (Pin 34 del LQFP48) |
| **SWCLK** | Pin `PA14` (Pin 37 del LQFP48) |
| **GND** | Plano de Tierra / GND |
| **3.3V** | VCC 3.3V |

---

## 6. 💻 Comando para Desbloquear y Borrar Chip vía ST-Link

### Usando STM32CubeProgrammer CLI:
```bash
STM32_Programmer_CLI.exe -c port=SWD mode=UR -readunprotect
```

### Usando OpenOCD:
```bash
openocd -f interface/stlink.cfg -f target/stm32f1x.cfg -c "init; reset halt; stm32f1x unlock 0; reset halt; exit"
```
