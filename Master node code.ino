#include <WiFi.h>
#include <WebServer.h>
#include <PubSubClient.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <ArduinoJson.h>

// WiFi
const char* ssid = "Galaxy F22 531A";
const char* password = "22222222";

// MQTT
const char* mqtt_server = "broker.hivemq.com";
const char* mqtt_topic = "solar/dust";
WiFiClient espClient;
PubSubClient mqttClient(espClient);

// Telegram
WiFiClientSecure secured_client;
const char* BOT_TOKEN = "put here your telegram bot token";
const char* CHAT_ID = "put here your chat ID";
UniversalTelegramBot bot(BOT_TOKEN, secured_client);

// Weather API
String weatherApiKey = "b793505fbbec45d099083007252807";
String city = "Colombo";
String weatherApiUrl = "http://api.weatherapi.com/v1/current.json?key=" + weatherApiKey + "&q=" + city;

// NTP
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 19800, 60000);

// Web Server
WebServer server(80);

// System status
String statusMessage = "✅ System Online — Waiting for MQTT Data";

// Function to check weather
bool isClearWeather() {
  HTTPClient http;
  http.begin(weatherApiUrl);
  int httpCode = http.GET();

  if (httpCode == 200) {
    String payload = http.getString();
    DynamicJsonDocument doc(1024);
    deserializeJson(doc, payload);
    String condition = doc["current"]["condition"]["text"];
    return !(condition.indexOf("rain") >= 0 || condition.indexOf("Rain") >= 0);
  } else {
    return false;
  }
}

// Send Telegram Message
void sendTelegramMessage(String message) {
  bot.sendMessage(CHAT_ID, message, "");
}

// MQTT Callback
void callback(char* topic, byte* payload, unsigned int length) {
  String msg;
  for (int i = 0; i < length; i++) msg += (char)payload[i];

  Serial.println("📥 MQTT Message: " + msg);
  timeClient.update();
  int hour = timeClient.getHours();

  if (hour >= 7 && hour <= 17) {
    if (msg == "dust_detected") {
      if (isClearWeather()) {
        sendTelegramMessage("⚠️ Dust detected on solar panels!");
        statusMessage = "⚠️ Dust detected. Telegram sent.";
      } else {
        statusMessage = "🌧 Dust detected, but it's raining. No alert sent.";
      }
    } else if (msg == "dust_cleared") {
      sendTelegramMessage("✅ Dust cleared.");
      statusMessage = "✅ Dust cleared. System reset.";
    }
  } else {
    statusMessage = "⏰ Outside 7AM–5PM. Message ignored.";
  }
}

// Serve HTML Page
void handleRoot() {
  String html = "<!DOCTYPE html><html><head><title>Solar Panel Monitor</title>";
  html += "<meta http-equiv='refresh' content='5' />";
  html += "<style>body{font-family:Arial;text-align:center;margin-top:50px;}</style></head><body>";
  html += "<h1>Solar Dust Monitoring Dashboard</h1>";
  html += "<h2>Status: " + statusMessage + "</h2>";
  html += "<p>Last update: " + timeClient.getFormattedTime() + "</p>";
  html += "</body></html>";

  server.send(200, "text/html", html);
}

void setup() {
  Serial.begin(115200);
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println(" ✅ Connected");

  timeClient.begin();
  timeClient.update();
  secured_client.setInsecure();

  mqttClient.setServer(mqtt_server, 1883);
  mqttClient.setCallback(callback);

  while (!mqttClient.connected()) {
    if (mqttClient.connect("ESP32MasterNode")) {
      mqttClient.subscribe(mqtt_topic);
    } else {
      delay(2000);
    }
  }

  server.on("/", handleRoot);
  server.begin();
  Serial.println("🚀 Web Server running at http://" + WiFi.localIP().toString());
}

void loop() {
  mqttClient.loop();
  timeClient.update();
  server.handleClient();
}
