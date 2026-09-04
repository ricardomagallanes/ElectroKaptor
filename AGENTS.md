# 🔒 DIRECTIVA MANDATORIA DE SEGURIDAD Y PROTECCIÓN DE DATOS SENSIBLES

**REGLA DE CUMPLIMIENTO OBLIGATORIO E INVIOLABLE EN TODO EL REPOSITORIO**

---

## 🚫 1. Prohibición Absoluta de Publicar EUIs Reales
- **DevEUI, AppEUI, JoinEUI:** Queda estrictamente prohibido escribir, registrar o subir a control de versiones (Git), documentación o archivos markdown cualquier dirección EUI real extraída de hardware, etiquetas o módulos radio.
- **Enmascaramiento Obligatorio:** Todo EUI en archivos de documentación, ejemplos o comentarios de código debe figurar sanitizado mediante marcadores genéricos (ejemplo: `<DEVEUI_PLACA_ORIGINAL>`, `0000000000000000` o `XX:XX:XX:XX:XX:XX:XX:XX`).

---

## 🔑 2. Prohibición de Claves, API Keys y Credenciales
- **Claves Criptográficas y Licencias:** Ninguna `AppKey`, `NwkSKey`, `AppSKey`, Licencia Heltec, token de API, contraseña o clave privada debe incluirse jamás en archivos públicos o rastreados por Git.
- **Uso de Archivos Seguros:** Todas las credenciales sensibles deben residir únicamente en archivos locales ignorados por Git (`Credentials.h`, `.env`). La plantilla en el repositorio debe utilizar siempre valores de plantilla o ceros (`Credentials.example.h`).

---

## 🏢 3. Prohibición de Datos Corporativos e Información Confidencial
- **Nombres de Empresa y Clientes:** Queda prohibido incluir nombres de empresas, clientes, razones sociales o datos comerciales propietarios en la documentación, código fuente o mensajes de commit.
- **Anonimización:** Usar referencias genéricas como *"Placa Original"*, *"Medidor de Campo"*, *"Servidor LNS"*, etc.

---

## 🔍 4. Auditoría Automática Antes de Cada Commit
Antes de realizar cualquier commit (`git commit`) o crear/modificar archivos en el proyecto, el sistema debe auditar automáticamente los cambios para garantizar que:
1. No existan cadenas de 16 caracteres hexadecimales correspondientes a EUIs reales.
2. No existan claves o AppKeys expuestas.
3. No existan nombres corporativos o referencias sensibles.

---

## 🚫 5. Prohibición Absoluta de Valores Hardcodeados, Ficticios o Inicializaciones Distintas de Cero
- **Cero Hardcoding:** Queda terminantemente prohibido inicializar, asignar por defecto, estimar, simular o hardcodear cualquier parámetro de medición física (tensión, corriente, factor de potencia, energía, demanda, batería, temperatura, frecuencia).
- **Inicialización Estricta en Cero:** Toda variable o estructura de medición (`MeterData`) debe inicializarse estrictamente en 0 (`0x00`).
- **Valores Reales Únicamente:** Si el medidor no transmite o no se decodifica fehacientemente un dato real a través de su interfaz de comunicación física/óptica, el valor reportado debe permanecer indefectiblemente en 0. Nunca se debe inferir o asignar un valor ficticio o nominal (por ejemplo, asignar 220V o 230V por inferencia).

