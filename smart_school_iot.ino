/*
  Smart School IoT
  ESP32 + RC522 RFID + DHT22
*/

#include <WiFi.h>
#include <HTTPClient.h>
#include <SPI.h>
#include <MFRC522.h>
#include "DHT.h"

#define SS_PIN 5
#define RST_PIN 22
#define DHT_PIN 4
#define DHT_TYPE DHT22
#define BUZZER_PIN 27
#define LED_GREEN 26
#define LED_RED 25

const char* WIFI_SSID = "IAP4";
const char* WIFI_PASSWORD = "123@Muhura";

// Replace 192.168.1.10 with the LAN IP of the computer running Flask.
const char* SERVER_URL = "http://192.168.1.10:5000";

MFRC522 rfid(SS_PIN, RST_PIN);
DHT dht(DHT_PIN, DHT_TYPE);

unsigned long lastTelemetry = 0;
const unsigned long TELEMETRY_INTERVAL = 10000;

void beep(int ms=120) {
  digitalWrite(BUZZER_PIN, HIGH);
  delay(ms);
  digitalWrite(BUZZER_PIN, LOW);
}

String uidToString() {
  String uid = "";
  for (byte i = 0; i < rfid.uid.size; i++) {
    if (rfid.uid.uidByte[i] < 0x10) uid += "0";
    uid += String(rfid.uid.uidByte[i], HEX);
  }
  uid.toUpperCase();
  return uid;
}

void connectWiFi() {
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to Wi-Fi");
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 30) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("Wi-Fi connected. ESP32 IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("Wi-Fi connection failed.");
  }
}

void sendAttendance(String uid) {
  if (WiFi.status() != WL_CONNECTED) return;

  HTTPClient http;
  String url = String(SERVER_URL) + "/api/attendance/scan";
  http.begin(url);
  http.addHeader("Content-Type", "application/json");

  String body = "{\"rfid_uid\":\"" + uid + "\"}";
  int code = http.POST(body);
  String response = http.getString();

  Serial.print("Attendance HTTP: ");
  Serial.println(code);
  Serial.println(response);

  if (code == 200) {
    digitalWrite(LED_GREEN, HIGH);
    beep(100);
    delay(100);
    beep(100);
    digitalWrite(LED_GREEN, LOW);
  } else {
    digitalWrite(LED_RED, HIGH);
    beep(500);
    digitalWrite(LED_RED, LOW);
  }
  http.end();
}

void sendTelemetry(float temperature, float humidity) {
  if (WiFi.status() != WL_CONNECTED) return;

  HTTPClient http;
  String url = String(SERVER_URL) + "/api/telemetry";
  http.begin(url);
  http.addHeader("Content-Type", "application/json");

  String body = "{\"temperature\":" + String(temperature, 2) +
                ",\"humidity\":" + String(humidity, 2) +
                ",\"device_id\":\"ESP32-CLASSROOM-01\"}";

  int code = http.POST(body);
  Serial.print("Telemetry HTTP: ");
  Serial.println(code);
  http.end();
}

void setup() {
  Serial.begin(115200);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(LED_GREEN, OUTPUT);
  pinMode(LED_RED, OUTPUT);

  SPI.begin();
  rfid.PCD_Init();
  dht.begin();

  connectWiFi();

  Serial.println("Smart School IoT ready.");
  Serial.println("Scan an RFID card...");
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    connectWiFi();
  }

  // RFID
  if (rfid.PICC_IsNewCardPresent() && rfid.PICC_ReadCardSerial()) {
    String uid = uidToString();
    Serial.print("RFID UID: ");
    Serial.println(uid);
    sendAttendance(uid);

    rfid.PICC_HaltA();
    rfid.PCD_StopCrypto1();
  }

  // DHT22 telemetry
  if (millis() - lastTelemetry >= TELEMETRY_INTERVAL) {
    lastTelemetry = millis();

    float humidity = dht.readHumidity();
    float temperature = dht.readTemperature();

    if (!isnan(humidity) && !isnan(temperature)) {
      Serial.print("Temperature: ");
      Serial.print(temperature);
      Serial.print(" C, Humidity: ");
      Serial.print(humidity);
      Serial.println(" %");

      sendTelemetry(temperature, humidity);
    } else {
      Serial.println("DHT22 read failed.");
    }
  }

  delay(50);
}
