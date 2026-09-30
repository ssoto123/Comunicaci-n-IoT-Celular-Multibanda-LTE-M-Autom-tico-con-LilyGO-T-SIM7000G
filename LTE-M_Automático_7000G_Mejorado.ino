/**************************************************************
 * Código de Conexión Celular (LTE-M / Automático) y MQTT
 * Placa: LilyGO T-SIM7000G
 * 
 * Funciones:
 * - Conexión automática (prioriza 4G IoT, baja a 2G).
 * - Conexión a broker MQTT público.
 * - Control remoto de LED.
 * - Lectura de sensor analógico.
 * - Geolocalización por LBS (Torres Celulares).
 * - Consulta de Saldo/Megas vía USSD.
 * - [NUEVO] Detección de Tecnología de Red (2G vs LTE-M).
 * - [NUEVO] Envío de tecnología de red en el JSON.
 **************************************************************/

// ============================================================
// 1. DEFINICIONES DE HARDWARE Y LIBRERÍAS
// ============================================================
#define TINY_GSM_MODEM_SIM7000

#define SerialMon Serial   
#define SerialAT Serial1   

#include <TinyGsmClient.h>
#include <PubSubClient.h>

// ============================================================
// 2. CONFIGURACIÓN DE RED Y MQTT
// ============================================================
#define TINY_GSM_USE_GPRS true  
#define TINY_GSM_USE_WIFI false 

// Credenciales APN (Telcel)
const char apn[]      = "internet.itelcel.com";     
const char gprsUser[] = "itelcel";
const char gprsPass[] = "itelcel";

const char* broker = "broker.hivemq.com";

const char* topicLed       = "GsmClientTest/led";       
const char* topicInit      = "GsmClientTest/init";      
const char* topicLedStatus = "GsmClientTest/ledStatus"; 
const char* topicDatos     = "GsmClientTest/telemetria"; 
const char* topicSaldo     = "GsmClientTest/saldo"; 

// ============================================================
// 3. CREACIÓN DE OBJETOS Y VARIABLES
// ============================================================
TinyGsm        modem(SerialAT); 
TinyGsmClient  client(modem);   
PubSubClient   mqtt(client);    

#define LED_PIN 12      
#define PWR_PIN 4       
#define PIN_SENSOR 34   

int ledStatus = LOW;    
uint32_t lastReconnectAttempt = 0; 
String imei = "";       
uint32_t lastDataSend = 0;

// [NUEVO] Variable global para almacenar si estamos en 2G o 4G
String tipoDeRedActual = "Desconocida"; 

// ============================================================
// 4. FUNCIONES DE RETROALIMENTACIÓN MQTT
// ============================================================
void mqttCallback(char* topic, byte* payload, unsigned int len) {
  SerialMon.print("Mensaje recibido [");
  SerialMon.print(topic);
  SerialMon.print("]: ");
  
  for (int i = 0; i < len; i++) {
    SerialMon.print((char)payload[i]);
  }
  SerialMon.println();

  if (String(topic) == topicLed) {
    ledStatus = !ledStatus;            
    digitalWrite(LED_PIN, ledStatus);  
    mqtt.publish(topicLedStatus, ledStatus ? "1" : "0");
  }
}

boolean mqttConnect() {
  SerialMon.print("Conectando al broker MQTT: ");
  SerialMon.print(broker);
  boolean status = mqtt.connect(imei.c_str());

  if (status == false) {
    SerialMon.println(" -> Falló");
    return false;
  }
  SerialMon.println(" -> ¡Éxito!");
  mqtt.publish(topicInit, "Dispositivo Maestro Conectado"); 
  mqtt.subscribe(topicLed);                                 
  return mqtt.connected();
}

// ============================================================
// 5. [NUEVO] FUNCIÓN PARA SABER SI ESTAMOS EN 2G O 4G
// ============================================================
void identificarTecnologiaRed() {
  SerialMon.print("Consultando tecnología de red al módem... ");
  
  // Enviamos el comando de Información de Sistema (System Information)
  modem.sendAT("+CPSI?");
  
  // Esperamos la respuesta "+CPSI: " por hasta 2 segundos
  if (modem.waitResponse(2000L, "+CPSI: ") == 1) {
    // Leemos el resto de la línea
    String respuesta = modem.stream.readStringUntil('\n');
    respuesta.trim();
    
    // Limpiamos el 'OK' final que el módem manda para no trabar futuros comandos
    modem.waitResponse(); 

    SerialMon.println("¡Información obtenida!");
    SerialMon.println("Detalle crudo: " + respuesta);
    SerialMon.println("======================================");

    // Parseamos la respuesta para hacerla fácil de leer
    if (respuesta.startsWith("GSM")) {
      tipoDeRedActual = "2G (GPRS)";
      SerialMon.println(">>> CONECTADO A: 2G (GPRS Clásico) <<<");
    } else if (respuesta.startsWith("LTE CAT-M1")) {
      tipoDeRedActual = "4G (LTE-M)";
      SerialMon.println(">>> CONECTADO A: 4G (LTE-M IoT)    <<<");
    } else if (respuesta.startsWith("LTE NB-IOT")) {
      tipoDeRedActual = "4G (NB-IoT)";
      SerialMon.println(">>> CONECTADO A: 4G (NB-IoT)       <<<");
    } else {
      // Si responde otra cosa, extraemos la primera palabra antes de la coma
      int comaIndex = respuesta.indexOf(',');
      if (comaIndex > 0) {
        tipoDeRedActual = respuesta.substring(0, comaIndex);
      } else {
        tipoDeRedActual = respuesta;
      }
      SerialMon.println(">>> CONECTADO A: " + tipoDeRedActual + " <<<");
    }
    SerialMon.println("======================================\n");
  } else {
    SerialMon.println("Fallo al obtener información.");
    modem.waitResponse(); // Limpiar el buffer por si acaso
  }
}

// ============================================================
// 6. FUNCIÓN DE CONSULTA USSD (Saldo)
// ============================================================
void consultarSaldo() {
  SerialMon.println("\n--- SOLICITANDO SALDO A LA RED ---");
  String respuestaUSSD = modem.sendUSSD("*133#");
  
  if (respuestaUSSD.length() > 0) {
    SerialMon.println("Respuesta del Operador:");
    SerialMon.println(respuestaUSSD);
    if (mqtt.connected()) mqtt.publish(topicSaldo, respuestaUSSD.c_str());
  } else {
    SerialMon.println("Sin respuesta USSD.");
  }
  SerialMon.println("----------------------------------\n");
}

// ============================================================
// 7. SETUP 
// ============================================================
void setup() {
  SerialMon.begin(115200);
  delay(10);

  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH);
  pinMode(PIN_SENSOR, INPUT);

  pinMode(PWR_PIN, OUTPUT);
  digitalWrite(PWR_PIN, HIGH); 
  delay(1000);                 
  digitalWrite(PWR_PIN, LOW);  

  SerialMon.println("Esperando a que el módem inicie...");
  SerialAT.begin(115200, SERIAL_8N1, 26, 27);
  delay(6000); 

  modem.restart(); 
  imei = modem.getIMEI(); 
  SerialMon.println("IMEI del equipo: " + imei);

  // MODO AUTOMÁTICO DE RED
  SerialMon.println("Configurando el radio en modo Automático (Prioridad 4G, respaldo 2G)...");
  modem.setNetworkMode(2);  // 2 = Automático
  modem.setPreferredMode(3); // 3 = LTE-M > NB-IoT > GSM

  SerialMon.println("Buscando red Telcel...");
  if (!modem.waitForNetwork()) {
    SerialMon.println(" -> Falló el registro en red.");
    delay(10000);
    return;
  }
  SerialMon.println(" -> Red encontrada con éxito");

  // [NUEVO] ¡Averiguar si conectó en 2G o 4G!
  identificarTecnologiaRed();

  // Consultar Saldo
  consultarSaldo();

  SerialMon.print("Conectando al APN: ");
  SerialMon.print(apn);
  if (!modem.gprsConnect(apn, gprsUser, gprsPass)) {
    SerialMon.println(" -> Falló la conexión de datos (IP).");
    delay(10000);
    return;
  }
  SerialMon.println(" -> ¡IP asignada!");
  delay(3000);

  mqtt.setServer(broker, 1883);
  mqtt.setCallback(mqttCallback);
}

// ============================================================
// 8. LOOP 
// ============================================================
void loop() {
  if (!modem.isNetworkConnected()) {
    SerialMon.println("Red celular perdida. Recuperando...");
    if (!modem.waitForNetwork(180000L, true)) return;
    
    // Si perdimos la red y volvió a conectar, verificamos en qué conectó esta vez
    identificarTecnologiaRed(); 

    if (!modem.isGprsConnected()) {
      modem.gprsConnect(apn, gprsUser, gprsPass);
    }
  }

  if (!mqtt.connected()) {
    uint32_t t = millis();
    if (t - lastReconnectAttempt > 10000L) {
      lastReconnectAttempt = t;
      if (mqttConnect()) lastReconnectAttempt = 0; 
    }
    delay(100);
    return; 
  }

  mqtt.loop();

  // ============================================================
  // ENVÍO DE TELEMETRÍA (JSON) Cada 30 Segundos
  // ============================================================
  if (millis() - lastDataSend > 30000L) {
    lastDataSend = millis();

    int valorSensor = analogRead(PIN_SENSOR);
    float voltaje = (valorSensor / 4095.0) * 3.3; 

    float lat = 0.0, lon = 0.0, accuracy = 0.0;
    int year, month, day, hour, min, sec;
    
    String jsonPayload = "{";
    jsonPayload += "\"dispositivo\":\"" + imei + "\",";
    jsonPayload += "\"sensor_raw\":" + String(valorSensor) + ",";
    jsonPayload += "\"voltaje\":" + String(voltaje, 2) + ",";
    
    // [NUEVO] Inyectamos la tecnología de red actual en el JSON
    jsonPayload += "\"red\":\"" + tipoDeRedActual + "\",";

    if (modem.getGsmLocation(&lat, &lon, &accuracy, &year, &month, &day, &hour, &min, &sec)) {
      jsonPayload += "\"latitud\":" + String(lat, 6) + ",";
      jsonPayload += "\"longitud\":" + String(lon, 6) + ",";
      jsonPayload += "\"precision_mts\":" + String(accuracy);
    } else {
      jsonPayload += "\"latitud\":null,";
      jsonPayload += "\"longitud\":null,";
      jsonPayload += "\"precision_mts\":null";
    }
    jsonPayload += "}";

    SerialMon.println("Publicando:");
    SerialMon.println(jsonPayload);
    mqtt.publish(topicDatos, jsonPayload.c_str());
  }
}
