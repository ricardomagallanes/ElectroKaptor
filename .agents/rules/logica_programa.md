# 🔒 DIRECTIVA MANDATORIA DE COMPORTAMIENTO Y LÓGICA DEL PROGRAMA

**REGLA DE CUMPLIMIENTO OBLIGATORIO E INVIOLABLE EN TODO EL REPOSITORIO**

---

## 💡 1. MÁQUINA DE ESTADOS Y CONTROL DE LEDS (Active-LOW)

En la placa principal **ME_LoRa_v3.6 (STM32F103C8T6)**, los LEDs deben respetar estrictamente las siguientes funciones:

1. **LED 2 (`PB1` - D5 Rojo Superior / Actividad y Transmisión):**
   - **Parpadear** de forma continua/intermitente durante la interrogación y recepción de la lectura óptica (`MiddeDTS27Reader` / `ElsterA150Reader`).
   - **Parpadear más rápido (ráfaga)** al validar y decodificar con éxito los registros del medidor.
   - **Permanecer encendido fijo continuo** durante todo el proceso de transmisión de datos por LoRaWAN (Trama 1 y Trama 2 hacia TTN).
   - **Apagarse** inmediatamente al concluir el envío de datos.

2. **LED 3 (`PB0` - D3 Rojo Medio / Indicador de Error):**
   - **Parpadear ante cualquier error o fallo:**
     - Fallo en lectura de la sonda óptica (sin respuesta, timeout).
     - Fallo de comunicación con el módem RAK3172.
     - Fallo de transmisión o pérdida de conexión LoRaWAN (sin Join).
     - Trampa de excepciones de hardware (`HardFault`, `BusFault`, `UsageFault`).

3. **LED 4 (`PB2` - D4 Rojo Inferior):** Reserva (permanece apagado).
4. **LED MCU (`PC13`):** Heartbeat periódico del microcontrolador.
5. **LED 1 (D6 Verde):** Alimentación 220V AC (control por hardware directo, sin GPIO).

---

## 📡 2. PROTOCOLO Y PARÁMETROS LORAWAN
- **Módem:** RAK3172 (AT Commands a 115200 8N1 por `PB6` TX / `PB7` RX con `USART1` remapeado).
- **Banda:** AU915 (`AT+BAND=6`).
- **Máscara:** Sub-banda 2 / FSB2 Canales 8-15 (`AT+MASK=0002`).
- **Activación:** OTAA (`AT+NJM=1`), Unconfirmed (`AT+CFM=0`), ADR habilitado (`AT+ADR=1`).
- **Puerto de Aplicación:** Puerto 10.
- **Formato de Payload:** Binario BitPacker (Mensaje 0: Telemetría Principal / Mensaje 1: Demandas y Reactiva).

---

## 🔬 3. PROTOCOLO ÓPTICO IEC 62056-21
- **Puerto Serie:** Bitbang de precisión en `PA3` (RX Pull-Up) y `PB10` (TX Push-Pull) a 300 baudios 7E1 ($3333.33\,\mu\text{s}/\text{bit}$).
- **Secuencia:** Sign-on `/?!\r\n` -> Identificación `/...` -> ACK `\x06000\r\n` -> Captura OBIS hasta `!`.
- Si no hay respuesta tras timeout, generar trama con `Estado = 2` (Sin Lectura) y parpadear LED 3 de error.
