#include <WiFi.h>
#include "esp_wifi.h"
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

// ---------- WIFI CONFIG ----------
const char* WIFI_SSID     = "AJ@Student";
const char* WIFI_PASSWORD = "Student2026";

// ---------- MQTT CONFIG ----------
const char* MQTT_SERVER   = "s7a2171c.ala.asia-southeast1.emqxsl.com";
const int   MQTT_PORT     = 8883;
const char* MQTT_USER     = "IHSAN";
const char* MQTT_PASSWORD = "afiyhensem";  // fill this in

const char* TELEMETRY_TOPIC = "campus/zone1/telemetry";
const char* COMMAND_TOPIC   = "campus/zone1/command";
const char* SENSOR_ID       = "ESP_ZONE_1";

const int LED_PIN = 2;

// ---------- WIFI STATE MACHINE ----------
enum WifiState { WIFI_DISCONNECTED, WIFI_CONNECTING, WIFI_CONNECTED };
WifiState wifiState = WIFI_DISCONNECTED;
unsigned long wifiReconnectAttempt = 0;
const unsigned long WIFI_RETRY_INTERVAL = 10000;

WiFiClientSecure espClient;
PubSubClient mqttClient(espClient);

unsigned long lastPublish = 0;
const unsigned long PUBLISH_INTERVAL = 5000;

void handleWifi() {
  switch (wifiState) {
    case WIFI_DISCONNECTED:
      if (millis() - wifiReconnectAttempt > WIFI_RETRY_INTERVAL) {
        Serial.println("[WiFi] Attempting to connect...");
        WiFi.disconnect(true);
        delay(100);
        WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
        wifiState = WIFI_CONNECTING;
        wifiReconnectAttempt = millis();
      }
      break;
    case WIFI_CONNECTING:
      if (WiFi.status() == WL_CONNECTED) {
        Serial.print("[WiFi] Connected! IP: ");
        Serial.println(WiFi.localIP());
        wifiState = WIFI_CONNECTED;
      } else if (millis() - wifiReconnectAttempt > WIFI_RETRY_INTERVAL) {
        Serial.println("[WiFi] Attempt timed out, retrying...");
        wifiState = WIFI_DISCONNECTED;
      }
      break;
    case WIFI_CONNECTED:
      if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[WiFi] Connection lost!");
        wifiState = WIFI_DISCONNECTED;
      }
      break;
  }
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  String message;
  for (unsigned int i = 0; i < length; i++) message += (char)payload[i];
  message.trim();
  message.toUpperCase();

  Serial.println("========================================");
  Serial.println("[MQTT] Command received on topic: " + String(topic));
  Serial.println("[MQTT] Payload: " + message);

  if (message == "ON") {
    digitalWrite(LED_PIN, HIGH);
    Serial.println("[Action] LED ON");
    Serial.println("[Verify] Pin read-back: " + String(digitalRead(LED_PIN)));
  } else if (message == "OFF") {
    digitalWrite(LED_PIN, LOW);
    Serial.println("[Action] LED OFF");
    Serial.println("[Verify] Pin read-back: " + String(digitalRead(LED_PIN)));
  }
  Serial.println("========================================");
}

void reconnectMqtt() {
  if (wifiState != WIFI_CONNECTED) return;
  if (!mqttClient.connected()) {
    Serial.println("[MQTT] Connecting...");
    String clientId = "ESP32Client-" + String(SENSOR_ID);
    if (mqttClient.connect(clientId.c_str(), MQTT_USER, MQTT_PASSWORD)) {
      Serial.println("[MQTT] Connected!");
      mqttClient.subscribe(COMMAND_TOPIC);
      Serial.println("[MQTT] Subscribed to " + String(COMMAND_TOPIC));
    } else {
      Serial.print("[MQTT] Failed, rc=");
      Serial.println(mqttClient.state());
    }
  }
}

void publishTelemetry() {
  float simTemp = 25.0 + random(-30, 60) / 10.0;
  float simHumidity = 50.0 + random(-100, 100) / 10.0;

  StaticJsonDocument<200> doc;
  doc["sensor_id"] = SENSOR_ID;
  doc["temperature"] = simTemp;
  doc["humidity"] = simHumidity;

  char buffer[200];
  serializeJson(doc, buffer);
  mqttClient.publish(TELEMETRY_TOPIC, buffer);
  Serial.println("[MQTT] Published: " + String(buffer));
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  WiFi.mode(WIFI_OFF);
  delay(500);
  WiFi.mode(WIFI_STA);
  esp_wifi_set_ps(WIFI_PS_NONE);
  delay(500);

  espClient.setInsecure();
  mqttClient.setServer(MQTT_SERVER, MQTT_PORT);
  mqttClient.setCallback(mqttCallback);
}

void loop() {
  handleWifi();

  if (wifiState == WIFI_CONNECTED) {
    if (!mqttClient.connected()) reconnectMqtt();
    mqttClient.loop();

    if (millis() - lastPublish > PUBLISH_INTERVAL) {
      lastPublish = millis();
      publishTelemetry();
    }
  }
}