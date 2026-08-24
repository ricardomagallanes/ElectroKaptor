# ElectroKaptor - Guía Completa de Conexión de Hardware: Sonda Infrarroja (IR) para Medidor Elster A150

Esta guía explica detalladamente cómo conectar la sonda óptico-infrarroja al microcontrolador **Heltec ESP32-S3 (WiFi LoRa 32 V3)**, tanto si usas una **sonda comercial IEC 62056-21** como si construyes una **sonda casera (DIY)** con componentes/módulos sueltos.

---

## 1. ¿Cómo funciona el puerto óptico del Elster A150?

El medidor Elster A150 cuenta con un ojo óptico magnético en la parte frontal que utiliza comunicación infrarroja (longitud de onda aproximada de **850nm - 940nm**).

El puerto óptico posee dos elementos:
1. **Un fototransistor / receptor IR en el medidor:** Para recibir datos desde tu circuito (TX).
2. **Un LED emisor IR en el medidor:** Para transmitir los datos de consumo hacia tu circuito (RX).

---

## 2. Opciones de Hardware para el Sensor IR

### Opción A: Sonda Óptica Comercial (Recomendada)
Si compras una **Sonda Óptica Serial TTL IEC 62056-21 / ANSI** (con imán incorporado):
* Posee **4 cables** directamente listos para conectar:
  * **VCC:** Conectar a `3.3V` o `5V` del ESP32-S3.
  * **GND:** Conectar a `GND` del ESP32-S3.
  * **TX de la sonda:** Conectar a **`GPIO 18` (RX)** del ESP32-S3.
  * **RX de la sonda:** Conectar a **`GPIO 17` (TX)** del ESP32-S3.

---

### Opción B: Módulo Infrarrojo o Componentes Caseros (DIY)

Si armas el circuito con un **Módulo IR** o **LEDs sueltos de 5mm**:

#### Componentes necesarios:
1. **Fototransistor Infrarrojo (Receptor RX):** Por ejemplo, un Fototransistor IR de 2 pines (o módulo receptor tipo TCRT5000 / Fotodiodo IR 940nm).
2. **LED Infrarrojo Emisor (Transmisor TX):** LED IR de 940nm de 2 pines.
3. **Resistencia de Pull-up:** `10kΩ` (para el receptor RX).
4. **Resistencia de Limitación:** `220Ω` (para el LED TX).

---

### Esquema de Conexión Eléctrica (Circuito DIY)

```
[Heltec ESP32-S3]                  [Circuito Sonda IR Casera]

  3.3V ----------------+------------ (VCC)
                       |
                     [10kΩ] (Pull-up)
                       |
  GPIO 18 (RX) --------+------------ Colector del Fototransistor IR (Receptor)
                                     Emisor del Fototransistor IR -----> GND

  GPIO 17 (TX) ---[220Ω]--- (Ánodo) LED IR Emisor (Transmisor)
                            (Cátodo) LED IR Emisor ---------------------> GND

  GND ------------------------------------------------------------------ GND
```

---

## 3. Tabla Resumen de Pines en Heltec ESP32-S3

| Pin Heltec ESP32-S3 | Componente | Descripción |
| :--- | :--- | :--- |
| **GPIO 18** | **Receptor IR (RX)** | Lee los pulsos ópticos del medidor Elster A150. |
| **GPIO 17** | **Emisor IR (TX)** | Envía peticiones/handshake al medidor (opcional según modo). |
| **3V3** | **VCC** | Alimentación de 3.3 voltios. |
| **GND** | **GND** | Masa / Tierra común. |

---

## 4. Consejos Importantes de Alineación Física
* **Alineación:** El LED emisor de tu circuito debe quedar apuntando directamente al receptor del medidor, y el fototransistor de tu circuito debe apuntar al LED emisor del medidor.
* **Imán de sujeción:** Te recomendamos usar un imán de neodimio en forma de aro (diámetro exterior ~32mm) para mantener la sonda firmemente acoplada al anillo metálico frontal del medidor Elster.
* **Luz ambiental:** Evita la luz solar directa directa sobre el fototransistor durante las pruebas, ya que la luz solar contiene infrarrojos que pueden saturar la lectura.
