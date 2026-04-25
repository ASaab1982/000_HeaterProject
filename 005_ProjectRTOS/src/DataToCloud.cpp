
#include "ProjectHeater.h" // To access your D_PRINT macros

const char broker[] = "d72dc8b632b04c8c91c4702a5b164d59.s1.eu.hivemq.cloud";
int        port     = 8883;
const char statusTopic[]  = "boilers/B1/status"; 
const char commandTopic[] = "boilers/B1/commands";
bool isDegradedMode = false; // Declare this at the top of your sketch

void TaskCloud(void *pvParameters) {
    // === STAGE 1: ONE-TIME INITIALIZATION ===
    // [2-WAY COMMUNICATION] Registration: Set the callback function to handle incoming MQTT messages
    mqttClient.onMessage(onMqttMessageReceived);
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
    mqttClient.setId("Arduino_Heater_Unit_B1");
    mqttClient.setCleanSession(true);      // Don't leave ghosts behind

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
        // [2-WAY COMMUNICATION] Subscription: Listen for messages on the dedicated command topic
        mqttClient.subscribe(commandTopic);
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
        // [2-WAY COMMUNICATION] Increased Responsiveness: The task now polls for messages every 100ms 
        // instead of waiting indefinitely for the scheduler's 30s periodic trigger.
        if (ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(100)) == pdPASS) {
            // If we were notified by the scheduler, perform the 30s periodic update
            sendBoilerData(); 
        }

        mqttClient.poll();

        if (!mqttClient.connected()) {
            Serial.println(F("[CLOUD] Connection lost! Retrying..."));
            mqttClient.connect(broker, port);
        }
    }
}


void sendBoilerData() {
    JsonDocument doc;
    doc["deviceId"] = "B1"; // This allows the UI to identify the boiler
    doc["dcMotorSpeed"] = g_dcMotorSpeed;
    doc["micAdc"] = g_micAdc;
    doc["thermistorTempC"] = g_thermistorTempC;
    doc["dhtTempC"] = g_dhtTempC;
    doc["dhtHumidity"] = g_dhtHumidity;
    doc["stepperAngleDeg"] = g_stepperAngleDeg;
    doc["servoPositionDeg"] = g_servoPositionDeg;
    doc["touched"] = touched;
    doc["systemHealth"] = systemHealth;
    // [2-WAY COMMUNICATION] Include heater state and target Home in the outgoing status JSON
    doc["heaterState"] = heaterState;
    doc["targetHomeTemp"] = targetHomeTemp;
    
    // We set the 'retained' flag to true. HiveMQ will now store this message
    // and give it to the server immediately when it connects/restarts.
    mqttClient.beginMessage(statusTopic, true);
    serializeJson(doc, mqttClient);
    mqttClient.endMessage(); // This is the moment the "Handoff" happens
    D_PRINTLN(" + Data Sent To HIVE");
}

// [2-WAY COMMUNICATION] Message Callback: Parses incoming JSON commands and updates local variables
void onMqttMessageReceived(int messageSize) {
    // Read the message into a string or buffer
    String message = "";
    while (mqttClient.available()) {
        message += (char)mqttClient.read();
    }

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, message);
    if (error) {
        Serial.print(F("deserializeJson() failed: "));
        Serial.println(error.f_str());
        return;
    }

    const char* command = doc["command"];
    if (command && strcmp(command, "heater") == 0) {
        const char* action = doc["action"];
        heaterState = (strcmp(action, "on") == 0);
        Serial.print("Heater is now: "); Serial.println(heaterState ? "ON" : "OFF");
        // [2-WAY COMMUNICATION] Immediate Feedback: Push status to UI as soon as variable changes
        sendBoilerData(); 
    } else if (command && strcmp(command, "temperature") == 0) {
        float value = doc["value"];

        // [SAFETY] Hardware-level clamp to enforce 5-30°C range (Defense in Depth)
        if (value < 5.0f) value = 5.0f;
        if (value > 30.0f) value = 30.0f;

        targetHomeTemp = value;
        Serial.print("New Target Home: "); Serial.println(targetHomeTemp);
        // [2-WAY COMMUNICATION] Immediate Feedback: Push status to UI as soon as variable changes
        sendBoilerData(); 
    }
}