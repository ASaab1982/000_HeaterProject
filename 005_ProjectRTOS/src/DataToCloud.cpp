
#include "ProjectHeater.h" // To access your D_PRINT macros

const char broker[] = "d72dc8b632b04c8c91c4702a5b164d59.s1.eu.hivemq.cloud";
int        port     = 8883;
const char topic[]  = "boilers/B1/status"; 
bool isDegradedMode = false; // Declare this at the top of your sketch

void TaskCloud(void *pvParameters) {
    // === STAGE 1: ONE-TIME INITIALIZATION ===
    // This happens only once when the task starts
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

  // adding a small delay to let the mcu breath

    delay(100);
    randomSeed(millis()); // to make random number always diffirent


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
    delay(100);


    // === STAGE 2: THE INFINITE LOOP ===
    for (;;) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        // Keep the connection alive
        mqttClient.poll();

        // Optional: Add auto-reconnect logic if connection drops
        if (!mqttClient.connected()) {
            Serial.println(F("[CLOUD] Connection lost! Retrying..."));
            mqttClient.connect(broker, port);
        }

        // Send data or do work
        sendBoilerData(); 
    }
}


void sendBoilerData() {
    JsonDocument doc;
    doc["dcMotorSpeed"] = g_dcMotorSpeed;
    doc["micAdc"] = g_micAdc;
    doc["thermistorTempC"] = g_thermistorTempC;
    doc["dhtTempC"] = g_dhtTempC;
    doc["dhtHumidity"] = g_dhtHumidity;
    doc["stepperAngleDeg"] = g_stepperAngleDeg;
    doc["servoPositionDeg"] = g_servoPositionDeg;
    doc["touched"] = touched;
    doc["systemHealth"] = systemHealth;
    
    mqttClient.beginMessage(topic);
    serializeJson(doc, mqttClient);
    mqttClient.endMessage(); // This is the moment the "Handoff" happens
    D_PRINTLN(" + Data Sent To HIVE");
}