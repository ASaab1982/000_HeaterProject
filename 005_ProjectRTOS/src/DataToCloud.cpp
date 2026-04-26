
#include "ProjectHeater.h" // To access your D_PRINT macros

const char broker[] = "d72dc8b632b04c8c91c4702a5b164d59.s1.eu.hivemq.cloud";
int        port     = 8883;
const char statusTopic[]  = "boilers/B1/status"; 
const char commandTopic[] = "boilers/B1/commands";
bool isDegradedMode = false; // Declare this at the top of your sketch wifi is down or Hive is down

// Simulation of wrong conenction to hive 
unsigned long lastPortSwap = 0;
bool simulationPortActive = false;
bool initializeCloud();
// enf of simulation a prameter 

void TaskCloud(void *pvParameters) {

    static bool alertSent = false;
    static unsigned long lastConnectionTime = millis(); // Track the last time we were OK
    const unsigned long REBOOT_THRESHOLD = 600000;      // 10 Minutes (in ms)


    initializeCloud();


    // === STAGE 2: THE INFINITE LOOP ===
    for (;;) {

        // Simulation of wrong conenction to hive 
            unsigned long currentMillis = millis();

            
        /*
        // 1. THE SIMULATION TIMER
        if (!simulationPortActive && (currentMillis - lastPortSwap >= 30000)) {
            // After 60 seconds of normal operation, switch to WRONG port
            port = 200; 
            simulationPortActive = true;
            lastPortSwap = currentMillis;
            mqttClient.stop(); // Force disconnect to apply the "broken" port
            Serial.println(F("\n[SIMULATION] Switching to PORT 200 (Connection should fail)"));
        } 
        else if (simulationPortActive && (currentMillis - lastPortSwap >= 30000)) {
            // After 10 seconds of "failure", switch back to CORRECT port
            port = 8883; 
            simulationPortActive = false;
            lastPortSwap = currentMillis;
            Serial.println(F("\n[SIMULATION] Switching back to PORT 8883 (Recovery starting)"));
        }
        // end of simulation
        */


        // This wakes up every 500ms (or whenever notified by the 30s scheduler)
        bool notified = (ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(500)) == pdPASS);

        if (!mqttClient.connected()) {
            // 1. ALERT LOGIC
            if (!alertSent) {
                Serial.println(F("\n[!] CONNECTION BROKEN: Starting 10min countdown to Reset..."));
                isDegradedMode = true;
                alertSent = true;
                vTaskDelay(pdMS_TO_TICKS(5000));
            }

            // 2. CHECK THE DURATION
            unsigned long offlineDuration = millis() - lastConnectionTime;
            
            if (offlineDuration >= REBOOT_THRESHOLD) {
                Serial.println(F("\n[!!!] 10 MINUTES OFFLINE. Restarting system for recovery..."));
                delay(1000); // Give Serial time to print
                
                // TRIGGER HARDWARE RESET (Arduino R4 specific)
                NVIC_SystemReset(); 
            }            
        }

        // --- IF CONNECTED ---
        // Update the timestamp so the 10-minute timer starts over
        lastConnectionTime = millis(); 
        alertSent = false;
        isDegradedMode = false;
        
        mqttClient.poll();
        
        if (notified) {
            sendBoilerData();
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

bool initializeCloud() {
    // [2-WAY COMMUNICATION] Registration
    mqttClient.onMessage(onMqttMessageReceived);

    // 1. WiFi Connection - Network 1
    Serial.print(F("[WIFI] Connecting to SSID: "));
    Serial.println(SECRET_SSID1);
    WiFi.begin(SECRET_SSID1, SECRET_PASS1);

    unsigned long wifiTimer = millis();
    while (WiFi.status() != WL_CONNECTED && (millis() - wifiTimer < 5000)) {
        delay(500);
        Serial.print(".");
    }

    // 2. Fallback to Network 2
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println(F("\n[WIFI] Trying SSID 2..."));
        WiFi.disconnect();
        delay(1000); 
        WiFi.begin(SECRET_SSID2, SECRET_PASS2);

        wifiTimer = millis();
        while (WiFi.status() != WL_CONNECTED && (millis() - wifiTimer < 5000)) {
            delay(500);
            Serial.print(".");
        }
    }

    // 3. IP Verification
    int ipTimeout = 0;
    while (WiFi.localIP() == IPAddress(0, 0, 0, 0) && ipTimeout < 10) {
        delay(100);
        ipTimeout++;
    }

    if (WiFi.localIP() == IPAddress(0, 0, 0, 0)) {
        Serial.println(F("\n[ERROR] No IP obtained."));
        return false; 
    }

    // 4. Sync Time (NTP)
    RTC.begin();
    unsigned long epochTime = WiFi.getTime();
    if (epochTime > 0) {
        RTCTime now(epochTime);
        RTC.setTime(now);
    }

    // 5. MQTT Credentials
    mqttClient.setUsernamePassword(SECRET_MQTT_USER, SECRET_MQTT_PASS);
    mqttClient.setId("Arduino_Heater_Unit_B1");
    mqttClient.setCleanSession(true);

    // 6. HiveMQ Connection Loop
    int attempts = 0;
    while (attempts < 10) {
        Serial.print(F("HiveMQ Attempt #"));
        Serial.println(attempts + 1);
        
        if (mqttClient.connect(broker, port)) {
            Serial.println(F("[SUCCESS] Connected to HiveMQ!"));
            mqttClient.subscribe(commandTopic);
            return true; 
        }
        attempts++;
        delay(1000);
    }

    return false;
}