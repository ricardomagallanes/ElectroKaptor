# ElectroKaptor ⚡📟

**ElectroKaptor** es un sistema de telemetría y lectura remota industrial para medidores de energía eléctrica (**MIDDE Trifásico DTS27**, **Elster A150** y futuros modelos), diseñado para operar tanto en la placa oficial **ME_LoRa_v3.6** basada en el microcontrolador **STM32F103C8T6** y módem **RAKwireless RAK3172 (RUI3)** como en la plataforma **Heltec ESP32-S3 (WiFi LoRa 32 V3)** con conectividad **LoRaWAN (TTN)** y sonda infrarroja (IR).

---

## 🚀 Características Principales

- **Arquitectura de Microcontroladores Dual:**
  - **Placa Oficial ME_LoRa_v3.6:** Microcontrolador ARM Cortex-M3 **STM32F103C8T6** (72 MHz) + Módem LoRaWAN **RAK3172** (STM32WLE5CC / Semtech SX1262) comunicado por USART1 Remapeado a 115200 baudios.
  - **Placa de Evaluación Heltec V3:** SoC **ESP32-S3** con transceptor SX1262 integrado.
- **Arquitectura Multimedidor Polimórfica:**
  - **MIDDE Trifásico DTS27:** Comunicación bidireccional IEC 62056-21 Modo C a **300 baudios 7E1** con volcado interactivo de registros OBIS.
  - **Elster Monofásico A150:** Captura por ráfaga espontánea a **2400 baudios 8N1** con desmodulación 4-a-8 bits y sincronismo atómico de 186 bytes.
- **Empaquetamiento Eficiente de Telemetría (BitPacker):**
  - **Trama 1 (Mensaje 0 - 15 bytes):** Tipo de medidor (Mono/Tri), Estado, Batería, Factor de Potencia (Cos $\phi$), Voltajes de Fase ($V_A, V_B, V_C$), Corrientes ($I_A, I_B, I_C$), Frecuencia y Energía Activa Importada Total.
  - **Trama 2 (Mensaje 1 - 16 bytes):** Demandas Máximas (Activa/Reactiva), Energías Reactivas Importada/Exportada y Energía Activa Exportada.
- **Conectividad LoRaWAN (OTAA) de Alta Resiliencia:**
  - Banda Regional **AU915** Sub-banda 2 (**FSB2**, Canales 8 al 15 / `AT+CHE=2`) hacia **The Things Network (TTN)** en **Data Rate 3 (DR3 - SF7)** y potencia máxima de emisión **20 dBm** (`AT+TXP=0`).
  - Sincronización asíncrona no bloqueante por eventos de radio (`+EVT:JOINED`, `+EVT:TX_DONE`), previniendo colisiones de comandos AT y saturación de la UART durante ventanas de escucha RX1/RX2.
- **Señalización Visual y Diagnóstico en LEDs (Active-LOW):**
  - **LED 2 (`PB1`):** Parpadea durante la lectura óptica, emite una ráfaga rápida (8 pulsos a 40 ms) tras decodificar con éxito, permanece encendido fijo durante la transmisión por radio y se **apaga inmediatamente** al finalizar el envío.
  - **LED 3 (`PB0`):** Indicador de error / fallo (parpadea ante fallos de lectura óptica o espera de enlace LoRaWAN).
  - **LED 4 (`PB2`):** Pin de reserva (apagado).
  - **LED MCU (`PC13`):** Heartbeat del microcontrolador STM32.
- **Control de Relays:** Control por pulsos en pines `PA8` (Relay Desconexión / Corte Remoto) y `PA9` (Relay Reconexión Remota).
- **Driver de Depuración Serial de Cero Overhead (`DebugSerial`):** Acceso directo por registros en `USART2` (`PA2` TX) a 115200 baudios sin uso de buffers dinámicos en RAM.

---

## ⚙️ Configuración del Medidor Activo

Para seleccionar el modelo de medidor a compilar en el firmware, se edita [`firmware/src/MeterConfig.h`](file:///c:/Users/nahuel/Documents/Antigravity/ElectroKaptor/firmware/src/MeterConfig.h):

```cpp
// Para medir MIDDE Trifásico DTS27:
// #define SELECTED_METER_MODEL METER_MODEL_MIDDE_DTS27

// Para medir Elster A150 (Monofásico):
#define SELECTED_METER_MODEL METER_MODEL_ELSTER_A150
```

---

## 📚 Documentación Técnica Detallada

- [📖 Lógica Completa del Programa y LEDs (`LOGICA_DEL_PROGRAMA.md`)](file:///c:/Users/nahuel/Documents/Antigravity/ElectroKaptor/documentacion/LOGICA_DEL_PROGRAMA.md)
- [⚡ Especificación Técnica: Medidor Monofásico Elster A150 (`medidor_elster_a150.md`)](file:///c:/Users/nahuel/Documents/Antigravity/ElectroKaptor/documentacion/medidor_elster_a150.md)
- [⚡ Especificación Técnica: Medidor Trifásico Midde DTS27 (`medidor_midde_dts27.md`)](file:///c:/Users/nahuel/Documents/Antigravity/ElectroKaptor/documentacion/medidor_midde_dts27.md)
- [🏗️ Guía de Arquitectura Multimedidor (`ARQUITECTURA_MULTIMEDIDOR.md`)](file:///c:/Users/nahuel/Documents/Antigravity/ElectroKaptor/documentacion/ARQUITECTURA_MULTIMEDIDOR.md)
- [🔌 Diagrama de Conexión y Pines (`diagrama_conexion.md`)](file:///c:/Users/nahuel/Documents/Antigravity/ElectroKaptor/docs/diagrama_conexion.md)

---

## 📁 Estructura del Repositorio

- [`firmware/`](file:///c:/Users/nahuel/Documents/Antigravity/ElectroKaptor/firmware/): Código fuente C++ para PlatformIO (entorno `stm32f103c8_original_board`).
  - [`BoardConfig.h`](file:///c:/Users/nahuel/Documents/Antigravity/ElectroKaptor/firmware/src/BoardConfig.h): Mapeo oficial de pines y periféricos de la PCB ME_LoRa_v3.6.
  - [`MeterConfig.h`](file:///c:/Users/nahuel/Documents/Antigravity/ElectroKaptor/firmware/src/MeterConfig.h): Selección de modelo de medidor a compilar.
  - [`IMeterReader.h`](file:///c:/Users/nahuel/Documents/Antigravity/ElectroKaptor/firmware/src/IMeterReader.h): Interfaz base polimórfica para lectores de medidores.
  - [`ElsterA150Reader.h / .cpp`](file:///c:/Users/nahuel/Documents/Antigravity/ElectroKaptor/firmware/src/ElsterA150Reader.h): Driver óptico para Elster A150 a 2400 baud 8N1.
  - [`MiddeDTS27Reader.h / .cpp`](file:///c:/Users/nahuel/Documents/Antigravity/ElectroKaptor/firmware/src/MiddeDTS27Reader.h): Lector óptico IEC 62056-21 Modo C a 300 baud 7E1.
  - [`LoRaWAN_Handler.h / .cpp`](file:///c:/Users/nahuel/Documents/Antigravity/ElectroKaptor/firmware/src/LoRaWAN_Handler.h): Controlador del módem RAK3172 AT (RUI3).
  - [`BitPacker.h / .cpp`](file:///c:/Users/nahuel/Documents/Antigravity/ElectroKaptor/firmware/src/BitPacker.h): Empaquetador binario de telemetría a nivel de bits.
  - [`Credentials.example.h`](file:///c:/Users/nahuel/Documents/Antigravity/ElectroKaptor/firmware/src/Credentials.example.h): Plantilla segura de credenciales LoRaWAN OTAA.
- [`ttn_decoder/`](file:///c:/Users/nahuel/Documents/Antigravity/ElectroKaptor/ttn_decoder/): Decodificador en Javascript (`decoder.js`) para The Things Network (TTN).
- [`documentacion/`](file:///c:/Users/nahuel/Documents/Antigravity/ElectroKaptor/documentacion/): Especificaciones de medidores, esquemas y lógica de programa.
- [`descripcion-trama-.txt`](file:///c:/Users/nahuel/Documents/Antigravity/ElectroKaptor/descripcion-trama-.txt): Desglose analítico y estructura binaria de las tramas del medidor DTS27.
- [`descripcion-comunicacion-esp32-vs-stm32.txt`](file:///c:/Users/nahuel/Documents/Antigravity/ElectroKaptor/descripcion-comunicacion-esp32-vs-stm32.txt): Análisis comparativo de arquitecturas ESP32 vs. STM32+RAK3172.

---

## 🔌 Asignación Oficial de Pines - Placa ME_LoRa_v3.6 (STM32F103C8T6 LQFP48)

| Periférico / Señal | Pin STM32 | Pin Físico LQFP48 | Función / Modo |
| :--- | :---: | :---: | :--- |
| **Puerto Óptico RX** | `PA3` | Pin 13 | Entrada GPIO con Pull-Up (Sonda IR / Fototransistor) |
| **Puerto Óptico TX** | `PB10` | Pin 21 | Salida GPIO (Sonda IR / LED Transmisor) |
| **RAK3172 UART2_RX** | `PB6` | Pin 42 | `USART1_TX` (Remapped) a 115200 baudios |
| **RAK3172 UART2_TX** | `PB7` | Pin 43 | `USART1_RX` (Remapped) a 115200 baudios |
| **RAK3172 NRST** | `PB8` | Pin 45 | Salida GPIO (Reset por Hardware del módem RAK) |
| **Debug Serial TX** | `PA2` | Pin 12 | Salida USART2 TX a 115200 baudios |
| **Relay Desconexión** | `PA8` | Pin 29 | Salida GPIO (Pulsos de Corte Remoto de Luz) |
| **Relay Conexión** | `PA9` | Pin 30 | Salida GPIO (Pulsos de Reconexión Remota de Luz) |
| **LED 2 (Actividad)** | `PB1` | Pin 19 | Salida Active-LOW (Lectura Óptica & Transmisión LoRa) |
| **LED 3 (Error)** | `PB0` | Pin 18 | Salida Active-LOW (Fallo / Alerta) |
| **LED 4 (Reserva)** | `PB2` | Pin 20 | Salida Active-LOW (Sin uso) |
| **LED MCU Onboard** | `PC13` | Pin 2 | Salida Active-LOW (Heartbeat del MCU) |

---

## 💡 Comportamiento de los LEDs de Estado

- **LED 2 (`PB1`):**
  - **Parpadeo intermitente:** Durante la lectura activa por el puerto óptico.
  - **Ráfaga rápida (8 pulsos de 40 ms):** Lectura óptica completada y decodificada exitosamente.
  - **Encendido fijo continuo:** Durante la emisión de las tramas LoRaWAN hacia TTN.
  - **Apagado:** Transmisión de telemetría finalizada con éxito (OK).
- **LED 3 (`PB0`):**
  - **Parpadeo:** Ante fallos de comunicación óptica, problemas en el Join OTAA o error en la transmisión por radio.

---

## 🚀 Compilación y Grabación con PlatformIO / OpenOCD

### Compilación:
```bash
cd firmware
python -m platformio run -e stm32f103c8_original_board
```

### Grabación por ST-Link SWD (OpenOCD):
```powershell
& "C:\Users\nahuel\.platformio\packages\tool-openocd\bin\openocd.exe" `
  -f "C:\Users\nahuel\.platformio\packages\tool-openocd\openocd\scripts\interface\stlink.cfg" `
  -f "C:\Users\nahuel\.platformio\packages\tool-openocd\openocd\scripts\target\stm32f1x.cfg" `
  -c "cortex_m reset_config sysresetreq" `
  -c "init" `
  -c "reset init" `
  -c "flash write_image erase .pio/build/stm32f103c8_original_board/firmware.elf" `
  -c "reset run" `
  -c "sleep 2000" `
  -c "shutdown"
```

---

## 📜 Licencia

Proyecto de telemetría, telegestión y monitoreo de medidores eléctricos.
