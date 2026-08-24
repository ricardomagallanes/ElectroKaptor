# ElectroKaptor: Guía de Instalación del Decodificador de Payload en The Things Network (TTN v3)

Este documento detalla los pasos para instalar y verificar el decodificador de carga útil (Payload Formatter) en la consola de **The Things Network (TTN LNS)**.

---

## 1. Credenciales LoRaWAN registradas

Asegúrate de registrar tu dispositivo Heltec ESP32-S3 en la consola de TTN con los siguientes parámetros extraídos de `configuracion LoraWan.txt`:

* **JoinEUI (AppEUI):** `6C4EEF66F47986A6`
* **AppKey:** `1F33A170A5F1FDA0AB697AAE2B95916B`
* **DevEUI:** Generado por la MAC del ESP32-S3 o ingresado manualmente desde el monitor serie.

---

## 2. Pasos para la Instalación en TTN Console

1. Inicia sesión en la **[Consola de The Things Network](https://console.cloud.thethings.network/)**.
2. Dirígete a **Applications** y selecciona tu aplicación de medición.
3. En el menú lateral izquierdo, haz clic en **Payload formatters** $\to$ **Uplink**.
4. En **Formatter type**, selecciona **Javascript**.
5. Abre el archivo [`decoder.js`](file:///c:/Users/nahuel/Documents/Antigravity/PayloadDecoder/ttn_decoder/decoder.js) de este repositorio, copia todo su contenido y pégalo en el editor de código de la consola TTN.
6. Haz clic en **Save changes**.

---

## 3. Verificación de Payload en TTN

Puedes probar el decodificador en la pestaña **Test** pasteando el siguiente payload binario de prueba:

* **Payload de prueba 1 (Grandes Clientes - Hexadecimal):** `38C5F88988988988980000000000000000000000000000`
* **Payload de prueba 2 (Medidor Trifásico MIDDE DTS27 - Validado):** `323131363431434530303030303030303030303030303030303030303441`
* **FPort:** `1`

### Resultado Decodificado Esperado (JSON Output):

```json
{
  "data": {
    "mensaje_nro": 0,
    "tipo_medidor_cod": 3,
    "tipo_medidor": "Grandes Clientes",
    "estado": "Normal",
    "bateria_pct": 24,
    "cosphi": "0.95",
    "voltaje_a": "109.2",
    "voltaje_b": "157.0",
    "voltaje_c": "78.5",
    "corriente_a": "15.70",
    "corriente_b": "30.72",
    "corriente_c": "0.00",
    "energia_activa_importada_kwh": "0.00"
  }
}
```
