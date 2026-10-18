//
// Cliente ESP32 que se conectará a broker HiveMQ
//
// Author: Antonio Alejandro Saenz Camero
// Afiliación: Universidad Nacional de Ingeniería (UNI)
// Curso: Internet de las Cosas (CBS37)
//

// Librerias
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include "DHT.h"

// Pines (segun tu diagrama de Wokwi)
#define DHT_PIN  15
#define DHT_TYPE DHT22
#define LED_PIN  2

// Configuracion WiFi
const char* WIFI_SSID = "Wokwi-GUEST";
const char* WIFI_PASS = "";

// MQTT Broker (HiveMQ public broker, TLS, no authentication)
const char* MQTT_HOST = "broker.hivemq.com";
const int   MQTT_PORT = 8883;

// Prefijo unico: el broker es publico. Debe ser IGUAL en el cliente Python.
// Los temas distinguen mayusculas y minusculas: "casa/LED" no es "casa/led".
const String TOPIC_PREFIX = "antonio-saenz/";
const String TOPIC_TEMP   = TOPIC_PREFIX + "casa/temperatura";  // ESP32 -> broker
const String TOPIC_LED    = TOPIC_PREFIX + "casa/LED";          // broker -> ESP32 (ON / OFF)

// CA certificado: Amazon Root CA 1
const char* CA_CERT = R"EOF(
-----BEGIN CERTIFICATE-----
MIIDQTCCAimgAwIBAgITBmyfz5m/jAo54vB4ikPmljZbyjANBgkqhkiG9w0BAQsF
ADA5MQswCQYDVQQGEwJVUzEPMA0GA1UEChMGQW1hem9uMRkwFwYDVQQDExBBbWF6
b24gUm9vdCBDQSAxMB4XDTE1MDUyNjAwMDAwMFoXDTM4MDExNzAwMDAwMFowOTEL
MAkGA1UEBhMCVVMxDzANBgNVBAoTBkFtYXpvbjEZMBcGA1UEAxMQQW1hem9uIFJv
b3QgQ0EgMTCCASIwDQYJKoZIhvcNAQEBBQADggEPADCCAQoCggEBALJ4gHHKeNXj
ca9HgFB0fW7Y14h29Jlo91ghYPl0hAEvrAIthtOgQ3pOsqTQNroBvo3bSMgHFzZM
9O6II8c+6zf1tRn4SWiw3te5djgdYZ6k/oI2peVKVuRF4fn9tBb6dNqcmzU5L/qw
IFAGbHrQgLKm+a/sRxmPUDgH3KKHOVj4utWp+UhnMJbulHheb4mjUcAwhmahRWa6
VOujw5H5SNz/0egwLX0tdHA114gk957EWW67c4cX8jJGKLhD+rcdqsq08p8kDi1L
93FcXmn/6pUCyziKrlA4b9v7LWIbxcceVOF34GfID5yHI9Y/QCB/IIDEgEw+OyQm
jgSubJrIqg0CAwEAAaNCMEAwDwYDVR0TAQH/BAUwAwEB/zAOBgNVHQ8BAf8EBAMC
AYYwHQYDVR0OBBYEFIQYzIU07LwMlJQuCFmcx7IQTgoIMA0GCSqGSIb3DQEBCwUA
A4IBAQCY8jdaQZChGsV2USggNiMOruYou6r4lK5IpDB/G/wkjUu0yKGX9rbxenDI
U5PMCCjjmCXPI6T53iHTfIUJrU6adTrCC2qJeHZERxhlbI1Bjjt/msv0tadQ1wUs
N+gDS63pYaACbvXy8MWy7Vu33PqUXHeeE6V/Uq2V8viTO96LXFvKWlJbYK8U90vv
o/ufQJVtMVT8QtPHRh8jrdkPSHCa2XV4cdFyQzR1bldZwgJcJmApzyMZFo6IQ6XU
5MsI+yMRQ+hDKXJioaldXgjUkK642M4UwtBV8ob2xJNDd2ZhwLnoQdeXeGADbkpy
rqXRfboQnoZsG4q5WTP468SQvvG5
-----END CERTIFICATE-----
)EOF";

WiFiClientSecure secureClient;
PubSubClient mqtt(secureClient);
DHT dht(DHT_PIN, DHT_TYPE);

unsigned long lastPublish = 0;
const unsigned long PUBLISH_EVERY_MS = 5000;   // 5 segundos

void connectWiFi() {
  WiFi.begin(WIFI_SSID, WIFI_PASS, 6);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected");
}

// TLS needs the correct date/time to validate the certificate
void syncTime() {
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");
  while (time(nullptr) < 1700000000) {
    delay(300);
    Serial.print("*");
  }
  Serial.println("\nTime synced");
}

// Se ejecuta cuando llega un mensaje a un tema al que estamos suscritos
void onMessage(char* topic, byte* payload, unsigned int length) {
  String msg;
  for (unsigned int i = 0; i < length; i++) msg += (char)payload[i];
  msg.trim();
  msg.toUpperCase();
  Serial.printf("Comando recibido en %s: %s\n", topic, msg.c_str());

  if (String(topic) == TOPIC_LED) {
    if (msg == "ON") {
      digitalWrite(LED_PIN, HIGH);
      Serial.println("LED -> ENCENDIDO");
    } else if (msg == "OFF") {
      digitalWrite(LED_PIN, LOW);
      Serial.println("LED -> APAGADO");
    } else {
      Serial.println("Comando no reconocido (usa ON u OFF)");
    }
  }
}

void connectMQTT() {
  while (!mqtt.connected()) {
    Serial.print("Connecting to MQTT...");
    String clientId = "esp32-" + String(random(0xffff), HEX);
    if (mqtt.connect(clientId.c_str())) {   // no user/password on this broker
      Serial.println("connected");
      mqtt.subscribe(TOPIC_LED.c_str());    // volver a suscribirse en cada reconexion
      Serial.printf("Suscrito a %s\n", TOPIC_LED.c_str());
    } else {
      char tlsErr[100];
      int tlsCode = secureClient.lastError(tlsErr, sizeof(tlsErr));
      Serial.printf("failed, rc=%d, tls=%d (%s). Retrying in 5s\n",
                    mqtt.state(), tlsCode, tlsErr);
      delay(5000);
    }
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
  dht.begin();

  connectWiFi();
  syncTime();

  secureClient.setCACert(CA_CERT);
  mqtt.setServer(MQTT_HOST, MQTT_PORT);
  mqtt.setCallback(onMessage);
}

void loop() {
  if (!mqtt.connected()) connectMQTT();
  mqtt.loop();

  if (millis() - lastPublish >= PUBLISH_EVERY_MS) {
    lastPublish = millis();

    float t = dht.readTemperature();
    float h = dht.readHumidity();
    if (isnan(t) || isnan(h)) {
      Serial.println("DHT22 read failed");
      return;
    }

    // Mensaje en formato JSON (temperatura en C, humedad relativa en %)
    char payload[96];
    snprintf(payload, sizeof(payload),
             "{\"temperatura\":%.1f,\"humedad\":%.1f,\"unidad\":\"C\"}", t, h);

    bool ok = mqtt.publish(TOPIC_TEMP.c_str(), payload);
    Serial.printf("Publish %s -> %s [%s]\n",
                  TOPIC_TEMP.c_str(), payload, ok ? "OK" : "FAILED");
  }
}