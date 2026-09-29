# 📡 Guía Avanzada: Comunicación IoT Celular Multibanda (LTE-M / Automático) con LilyGO T-SIM7000G

¡Felicidades al equipo! Si están utilizando este código, significa que han superado las barreras eléctricas y de configuración básicas, logrando establecer un túnel de datos de alta velocidad sobre una red 4G IoT.

Este documento explica la arquitectura de nuestra conexión celular inteligente, utilizando el microcontrolador ESP32 (el cerebro) y el módulo SIM7000G (el radio-módem).

---

## 1. Evolución: De GPRS a LTE-M (La Nueva Conexión)

En versiones anteriores, dependíamos genéricamente de la red GPRS (2G). En esta versión, hemos configurado el radio en **Modo Automático con Preferencia IoT**. Pero, ¿qué significa este salto tecnológico?

### Diferencias Clave y Mejoras

*   **Latencia (El tiempo de reacción):** 
    *   *GPRS (2G):* Es como enviar una carta por correo postal. Tarda de 1 a 3 segundos en ir y regresar.
    *   *LTE-M (4G IoT):* Es como enviar un mensaje de WhatsApp. La latencia baja a unos ~100 milisegundos. (Por eso en el monitor serie ahora pueden ver cómo los mensajes de confirmación de los LEDs llegan en ráfagas casi instantáneas).
*   **Velocidad de Transmisión:**
    *   *GPRS:* Máximo de 80 kbps. Ideal para enviar números pequeños, pero se asfixia con datos pesados.
    *   *LTE-M:* Hasta 1 Mbps (Megabit por segundo). La autopista es mucho más ancha.
*   **Eficiencia Energética y Cobertura:** 
    *   *LTE-M* utiliza técnicas avanzadas de señal que le permiten penetrar mejor en interiores (sótanos, naves industriales). Además, negocia con la torre celular para "dormir" más profundamente, ahorrando muchísima más batería que el 2G.

### Estrategia de Conexión "Resiliente" (Modo Automático)
En lugar de forzar al módem a usar solo una red, el código actual le da **libertad de decisión**:
1. Busca primero las supercarreteras IoT (**Cat-M1 / NB-IoT**).
2. Si está en una zona rural o la torre de Telcel rechaza el protocolo IoT, automáticamente hace un fallback (plan de respaldo) y se conecta por la red clásica **2G (EDGE/GPRS)**. 
¡Esto garantiza que el dispositivo casi nunca se quede sin internet!

### 🚀 Nuevas Posibilidades con LTE-M para Proyectos de Maestría
Tener este ancho de banda y baja latencia nos permite escalar los proyectos:
1.  **Telemetría en Tiempo Real Automotriz:** A diferencia de NB-IoT, LTE-M soporta *Handover* (pasar de una antena a otra sin desconectarse mientras vas a 120 km/h). Ideal para rastreo de flotas.
2.  **Actualizaciones OTA (Over-The-Air):** El ancho de banda permite enviar un nuevo código binario (firmware) al ESP32 desde internet sin tener que ir a conectar el cable USB físicamente al campo.
3.  **Transmisión de Audio / Imágenes Ligeras:** Mientras que GPRS solo sirve para texto y números, LTE-M tiene la capacidad de transmitir pequeños clips de audio o fotos de baja resolución en casos de alarma.

---

## 2. Arquitectura de Hardware: El Ingeniero y el Chofer

La LilyGO T-SIM7000G son dos sistemas independientes en una misma placa:

*   **ESP32 (El Ingeniero):** Ejecuta el código de Arduino, pero no tiene antena celular.
*   **SIM7000G (El Chofer):** Tiene la antena LTE y el chip Telcel, pero necesita recibir órdenes para actuar.

### Esquema de Comunicación Interna (UART)

    [ ESP32 - El Ingeniero ]                  [ SIM7000G - El Chofer ]
                 |                                       |
    Pin 26 (TX)  |--- Transmite Datos / Comandos AT -->  | (RX)
                 |                                       |
    Pin 27 (RX)  |<-- Recibe Respuestas / Datos -------  | (TX)
                 |                                       |
    Pin 4 (PWR)  |--- Señal de Encendido (1s pulso) -->  | (Botón Power)
                 |                                       |
    Pin 12 (LED) |--- (Indicador Visual del ESP32)       |

---

## 3. Conceptos Teóricos: La Oficina de Correos (MQTT)

*   **MQTT Broker (broker.hivemq.com):** Es nuestra Oficina de Correos en la nube. 
*   **Tópicos (Topics):** Son los buzones. En lugar de que la computadora y el ESP32 se conecten directamente entre ellos (lo cual es bloqueado por las operadoras celulares), ambos se conectan a la Oficina de Correos. 
    *   La computadora *Publica* un mensaje en el buzón `GsmClientTest/led`.
    *   El ESP32 está *Suscrito* (escuchando) ese buzón, y cuando detecta la orden, enciende la luz.

---

## 4. Los Comandos AT (El idioma interno)

La librería TinyGSM traduce nuestro código en C++ a "Comandos AT" (texto plano) que viajan por los pines 26 y 27. Lo que hace el código en el setup() equivale a enviar esto:

*   `AT+CNMP=2`: Configura el radio en Automático (2G / 3G / 4G).
*   `AT+CMNB=3`: Configura la preferencia de red 4G a Cat-M1 y NB-IoT.
*   `AT+CGACT=1,1`: Activa el túnel de internet (Contexto PDP).

---

## 5. Guía Rápida de Pruebas

1.  Inserta una tarjeta SIM (ej. Telcel) y asegúrate de conectar la **Antena en el puerto LTE**.
2.  Asegúrate de estar alimentando la placa con una **batería LiPo 18650** conectada atrás. (El puerto USB solo no soporta los picos de energía al conectarse a 4G).
3.  Sube el código y abre el Monitor Serie. Espera a ver el mensaje de GPRS conectado y el Éxito en el MQTT.
4.  Entra a HiveMQ Web Client ([http://www.hivemq.com/demos/websocket-client/](http://www.hivemq.com/demos/websocket-client/)).
5.  **Conéctate** a la interfaz web.
6.  En **Publish**, escribe el Topic: `GsmClientTest/led` y presiona el botón Publish.
7.  ¡Observa el LED de la placa y la respuesta casi instantánea en el Monitor Serie!

---

## 6. Troubleshooting: Escenarios de Fallo Comunes

1.  **El ESP32 se reinicia constantemente (Brownout detector was triggered)**
    *   **Causa:** Al conectarse a la antena, el módem exige hasta 2 Amperios de golpe. El cable USB no da abasto.
    *   **Solución:** Conecta la batería 18650 a la placa. Actuará como un amortiguador de energía.
2.  **Se conecta a la red pero el MQTT dice "Falló" repetidamente**
    *   **Causa:** Alguien más en el mundo está usando exactamente el mismo `Client ID` en su código. HiveMQ expulsa a los dispositivos duplicados.
    *   **Solución:** El código ha sido actualizado para usar el número IMEI de la tarjeta como identificador único y evitar este problema.
3.  **Falla en "Conectando al APN"**
    *   **Causa:** El módem vio la torre celular, pero la caseta de cobro (APN) rechazó tu chip.
    *   **Solución:** Verifica que `internet.itelcel.com` sea el APN correcto para tu chip específico y que tengas datos activos.
4.  **"Timeout" o el Módem no responde al arrancar**
    *   **Causa:** Los pines seriales no están bien declarados o el módem no encendió físicamente.
    *   **Solución:** Revisa que el código mantenga la línea de inicio de SerialAT en los pines 26 y 27, y que el pulso en el pin 4 dure un segundo.
5.  **Se recibe la orden por MQTT, pero el LED no enciende**
    *   **Causa:** Todo el sistema celular e internet funciona, pero hay un problema eléctrico local en la placa.
    *   **Solución:** Verifica que la variable `LED_PIN` coincida con el pin físico del LED azul (Pin 12 en la T-SIM7000G).
