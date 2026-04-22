#include <Arduino.h>
#include <WiFiS3.h>
#include <ArduinoMqttClient.h>
#include <ArduinoJson.h>
#include <RTC.h>
#include "Secrets.h" // Make sure this is in your 'include' folder

const char broker[] = "d72dc8b632b04c8c91c4702a5b164d59.s1.eu.hivemq.cloud";
int        port     = 8883;
const char topic[]  = "boilers/B1/status"; 
bool isDegradedMode = false; // Declare this at the top of your sketch

WiFiSSLClient  wifiClient; 
MqttClient     mqttClient(wifiClient);

void sendBoilerData();



void setup() {
    Serial.begin(115200);
    
    // Give the Serial port 2 seconds to connect, then move on anyway
    unsigned long startWait = millis();
    while (!Serial && (millis() - startWait < 2000));
    Serial.println("[SYSTEM] Boiler B1 initialization started...");

    // 1. Start WiFi Connection - Try First Network
    Serial.print("[WIFI] Connecting to SSID: ");
    Serial.println(SECRET_SSID1);
    WiFi.begin(SECRET_SSID1, SECRET_PASS1);

    // Wait for the Physical Connection for up to 5 seconds
    unsigned long wifiTimer = millis();
    while (WiFi.status() != WL_CONNECTED && (millis() - wifiTimer < 5000)) {
        delay(500);
        Serial.print(".");
    }

    // 2. Fallback to Second Network if failed
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("\n[WIFI] First network timed out. Trying SSID: ");
        Serial.println(SECRET_SSID2);
        WiFi.disconnect();
        delay(1000); // Small buffer for radio reset
        WiFi.begin(SECRET_SSID2, SECRET_PASS2);

        wifiTimer = millis();
        while (WiFi.status() != WL_CONNECTED && (millis() - wifiTimer < 5000)) {
            delay(500);
            Serial.print(".");
        }
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("\n[WIFI] Connected to Router!");
    } else {
        Serial.println("\n[ERROR] All WiFi connection attempts failed.");
    }

    // 3. WAIT FOR TRUE IP ADDRESS (The Fix)
    Serial.print("[WIFI] Waiting for IP address");
    int ipTimeout = 0;
    while (WiFi.localIP() == IPAddress(0, 0, 0, 0) && ipTimeout < 10) {
        delay(100);
        Serial.print(".");
        ipTimeout++;
    }

    if (WiFi.localIP() == IPAddress(0, 0, 0, 0)) {
        isDegradedMode = true; 
        Serial.println("\n[WARNING] No IP. Entering DEGRADED MODE (Offline).");
    } else {
        isDegradedMode = false;
        Serial.print("\n[WIFI] Verified IP: ");
        Serial.println(WiFi.localIP());
    }


    // Initialize internal RTC and sync with Network Time (NTP)
    RTC.begin();
    unsigned long epochTime = WiFi.getTime();
    if (epochTime > 0) {
        RTCTime now(epochTime);
        RTC.setTime(now);
    }

    mqttClient.setUsernamePassword(SECRET_MQTT_USER, SECRET_MQTT_PASS);
    
// --- 10 SECOND RETRY LOOP connection to the HIVE Cloud---
    int attempts = 0;
    const int maxAttempts = 10;
    bool connected = false;
    Serial.println("Attempting to connect to HiveMQ Cloud...");
    while (attempts < maxAttempts) {
        Serial.print("Attempt #");
        Serial.print(attempts + 1);
        
        if (mqttClient.connect(broker, port)) {
            connected = true;
            break; // Exit the loop early because we succeeded!
        } else {
            Serial.print(" - Failed (Error ");
            Serial.print(mqttClient.connectError());
            Serial.println("). Retrying in 1s...");
            attempts++;
            delay(1000); // Wait 1 second before the next try
        }
    }

    if (connected) {
        Serial.println("SUCCESS: Connected to HiveMQ!");
    } else {
        Serial.println("-----------------------------------------");
        Serial.println("FATAL FAILURE: Could not connect after 10s.");
        Serial.println("Check credentials or Cluster URL.");
        Serial.println("-----------------------------------------");
        return; // This stops the setup, and loop() will check connection status
    }
}

void loop() {
    mqttClient.poll();

    static unsigned long prev = 0;
    if (millis() - prev > 60000) {
        prev = millis();
        
        // Get the current time from the internal RTC
        RTCTime now;
        RTC.getTime(now);

        sendBoilerData();

   // Display timestamped confirmation
        Serial.print("[");
        Serial.print(now.getYear()); Serial.print("-");
        
        // getMonth() returns an enum, so we cast it to int and add 1
        if ((int)now.getMonth() + 1 < 10) Serial.print("0");
        Serial.print((int)now.getMonth() + 1); Serial.print("-");
        
        if (now.getDayOfMonth() < 10) Serial.print("0");
        Serial.print(now.getDayOfMonth()); Serial.print(" ");
        
        if (now.getHour() < 10) Serial.print("0");
        Serial.print(now.getHour()); Serial.print(":");
        
        // FIX: Use getMinutes() instead of getMinute()
        if (now.getMinutes() < 10) Serial.print("0");
        Serial.print(now.getMinutes()); Serial.print(":");
        
        // FIX: Use getSeconds() instead of getSecond()
        if (now.getSeconds() < 10) Serial.print("0");
        Serial.print(now.getSeconds()); 
        
        Serial.println("] Data sent to Cloud.");
    }
}

void sendBoilerData() {
    JsonDocument doc;
    doc["id"] = "B1";
    doc["temp"] = 70.0; // Placeholders for now
    doc["state"] = "IDLE";

    mqttClient.beginMessage(topic);
    serializeJson(doc, mqttClient);
    mqttClient.endMessage();
}