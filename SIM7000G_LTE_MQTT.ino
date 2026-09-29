/**************************************************************
 * Código de Conexión Celular (LTE-M / Automático) y MQTT
 * Placa: LilyGO T-SIM7000G
 * 
 * Este código establece conexión con la red celular de forma 
 * automática (priorizando 4G IoT y bajando a 2G si es necesario),
 * se conecta a un broker MQTT público y permite controlar un LED
 * de forma remota casi en tiempo real.
 **************************************************************/

// ============================================================
// 1. DEFINICIONES DE HARDWARE Y LIBRERÍAS
// ============================================================
#define TINY_GSM_MODEM_SIM7000

#define SerialMon Serial   // Para imprimir mensajes en la computadora
#define SerialAT Serial1   // Para que el ESP32 hable internamente con el SIM7000G

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

// Configuración del Broker MQTT
const char* broker = "broker.hivemq.com";

// Tópicos
const char* topicLed       = "GsmClientTest/led";       
const char* topicInit      = "GsmClientTest/init";      
const char* topicLedStatus = "GsmClientTest/ledStatus"; 

// ============================================================
// 3. CREACIÓN DE OBJETOS Y VARIABLES
// ============================================================
TinyGsm        modem(SerialAT); 
TinyGsmClient  client(modem);   
PubSubClient   mqtt(client);    

#define LED_PIN 12      
#define PWR_PIN 4       

int ledStatus = LOW;    
uint32_t lastReconnectAttempt = 0; 
String imei = "";       

// ============================================================
// 4. FUNCIÓN CALLBACK (RECEPCIÓN DE MENSAJES)
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

// ============================================================
// 5. FUNCIÓN DE CONEXIÓN MQTT
// ============================================================
boolean mqttConnect() {
  SerialMon.print("Conectando al broker MQTT: ");
  SerialMon.print(broker);

  // Usamos el IMEI como Client ID único
  boolean status = mqtt.connect(imei.c_str());

  if (status == false) {
    SerialMon.println(" -> Falló la conexión al Broker");
    return false;
  }
  
  SerialMon.println(" -> ¡Éxito!");
  mqtt.publish(topicInit, "Dispositivo Maestro Conectado"); 
  mqtt.subscribe(topicLed);                                 
  return mqtt.connected();
}

// ============================================================
// 6. SETUP 
// ============================================================
void setup() {
  SerialMon.begin(115200);
  delay(10);

  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH);

  // ENCENDIDO DEL MÓDEM
  pinMode(PWR_PIN, OUTPUT);
  digitalWrite(PWR_PIN, HIGH); 
  delay(1000);                 
  digitalWrite(PWR_PIN, LOW);  

  SerialMon.println("Esperando a que el módem inicie...");

  SerialAT.begin(115200, SERIAL_8N1, 26, 27);
  delay(6000); 

  SerialMon.println("Inicializando módem celular...");
  modem.restart(); 
  
  String modemInfo = modem.getModemInfo();
  imei = modem.getIMEI(); 
  SerialMon.print("Info del Módem: ");
  SerialMon.println(modemInfo);
  SerialMon.print("IMEI del equipo: ");
  SerialMon.println(imei);

  // CONFIGURACIÓN DE BANDA CELULAR (MODO AUTOMÁTICO)
  SerialMon.println("Configurando el radio en modo Automático...");
  modem.setNetworkMode(2); 
  modem.setPreferredMode(3);
  SerialMon.println("Radio configurado. Buscando red Telcel...");

  if (!modem.waitForNetwork()) {
    SerialMon.println(" -> Falló. Revisa la antena y el chip.");
    delay(10000);
    return;
  }
  SerialMon.println(" -> Red encontrada con éxito");

  SerialMon.print("Conectando al APN: ");
  SerialMon.print(apn);
  if (!modem.gprsConnect(apn, gprsUser, gprsPass)) {
    SerialMon.println(" -> Falló la conexión de datos.");
    delay(10000);
    return;
  }
  SerialMon.println(" -> ¡Conexión de datos establecida!");

  // Pausa para estabilizar IP
  delay(3000);

  mqtt.setServer(broker, 1883);
  mqtt.setCallback(mqttCallback);
}

// ============================================================
// 7. LOOP 
// ============================================================
void loop() {
  
  if (!modem.isNetworkConnected()) {
    SerialMon.println("Red celular perdida. Intentando recuperar...");
    if (!modem.waitForNetwork(180000L, true)) { 
      SerialMon.println(" -> Falló la reconexión de red.");
      delay(10000);
      return;
    }
    SerialMon.println(" -> Red celular recuperada.");

    if (!modem.isGprsConnected()) {
      SerialMon.println("Datos caídos. Reconectando al APN...");
      if (!modem.gprsConnect(apn, gprsUser, gprsPass)) {
        SerialMon.println(" -> Falló la reconexión de datos.");
        delay(10000);
        return;
      }
      SerialMon.println(" -> Datos reconectados.");
    }
  }

  if (!mqtt.connected()) {
    SerialMon.println("=== MQTT NO CONECTADO ===");
    
    uint32_t t = millis();
    if (t - lastReconnectAttempt > 10000L) {
      lastReconnectAttempt = t;
      if (mqttConnect()) {
        lastReconnectAttempt = 0; 
      }
    }
    delay(100);
    return; 
  }

  mqtt.loop();
}
