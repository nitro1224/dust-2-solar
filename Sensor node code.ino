#include <WiFi.h>
#include <PubSubClient.h>

const char* ssid = "Galaxy F22 531A";
const char* password = "22222222";
const char* mqtt_server = "broker.hivemq.com";
const char* topic = "solar/dust";

WiFiClient espClient;
PubSubClient client(espClient);

// LDR pins (update pin numbers based on your wiring)
const int ldrPins[6] = {32, 33, 25, 34, 35, 26};  // 6 LDRs
#define CALIBRATION_BUTTON 14
#define RESET_BUTTON 27

int baseline = 0;
bool calibrated = false;
bool dustDetected = false;
bool detectionStopped = false;

void setup() {
  Serial.begin(115200);

  for (int i = 0; i < 6; i++) {
    pinMode(ldrPins[i], INPUT);
  }

  pinMode(CALIBRATION_BUTTON, INPUT_PULLUP);
  pinMode(RESET_BUTTON, INPUT_PULLUP);

  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\n✅ WiFi connected");

  client.setServer(mqtt_server, 1883);
}

void reconnectMQTT() {
  while (!client.connected()) {
    Serial.print("Connecting to MQTT...");
    if (client.connect("ESP32SensorNode")) {
      Serial.println("connected ✅");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" retrying in 2s...");
      delay(2000);
    }
  }
}

int readLDRAvg() {
  int sum = 0;
  for (int i = 0; i < 6; i++) {
    sum += analogRead(ldrPins[i]);
  }
  return sum / 6;
}

void loop() {
  if (!client.connected()) {
    reconnectMQTT();
  }
  client.loop();

  // Calibration
  if (digitalRead(CALIBRATION_BUTTON) == LOW) {
    baseline = readLDRAvg();
    calibrated = true;
    detectionStopped = false;
    dustDetected = false;
    Serial.println("📍 Calibration triggered");
    Serial.println("✅ Calibrated. System ready.");
    delay(1000); // debounce
  }

  // Reset
  if (digitalRead(RESET_BUTTON) == LOW) {
    calibrated = true;
    detectionStopped = false;
    dustDetected = false;
    client.publish(topic, "dust_cleared");
    Serial.println("🔄 System reset. Ready for detection.");
    delay(1000);
  }

  if (calibrated && !detectionStopped) {
    int currentAvg = readLDRAvg();
    Serial.println("LDR Avg: " + String(currentAvg) + " | Baseline: " + String(baseline));

    if (abs(currentAvg - baseline) > 200) {  // Sensitivity
      if (!dustDetected) {
        client.publish(topic, "dust_detected");
        Serial.println("⚠ Dust Detected - MQTT message sent.");
        dustDetected = true;
        detectionStopped = true;
      }
    }
  }

  delay(1000);
}