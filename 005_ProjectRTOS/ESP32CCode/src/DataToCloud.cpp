/*
 * DataToCloud.cpp — WiFi / MQTT cloud communication
 *
 * This file manages the full lifecycle of the Arduino's connection to the HiveMQ broker
 * over TLS (port 8883).  It handles:
 *   - WiFi connection with automatic fallback to a second SSID
 *   - NTP time synchronisation via the RTC
 *   - MQTT connection with Last Will and Testament so the UI detects unexpected drops
 *   - Periodic telemetry: all sensor readings, simulation state, and control variables
 *     are serialised to JSON and published to boilers/B1/status every 15 seconds
 *   - Incoming command parsing: heater on/off, target home temperature, manual override
 *     toggle, boiler setpoint, and real outdoor weather data from Open-Meteo via Node.js
 *   - Offline watchdog: if the MQTT connection is lost for 10 minutes the system resets
 *     via esp_restart() to attempt automatic recovery
 *
 * TaskCloud wakes every 500 ms to call mqttClient.poll() (keepalive + receive).
 * It is notified by TaskTimeScheduler every 15 s to trigger a telemetry publish.
 */

#include "ProjectHeater.h" // To access your D_PRINT macros

const char broker[] = "d72dc8b632b04c8c91c4702a5b164d59.s1.eu.hivemq.cloud";
int        port     = 8883;
const char statusTopic[]  = "boilers/ESP/status";
const char commandTopic[] = "boilers/ESP/commands";
bool isDegradedMode = false; // Declare this at the top of your sketch wifi is down or Hive is down

// Simulation of wrong conenction to hive 
unsigned long lastPortSwap = 0;
bool simulationPortActive = false;
bool initializeCloud();
// enf of simulation a prameter 

// FreeRTOS task — manages the MQTT connection and periodic telemetry.
// Calls initializeCloud() once on startup, then loops every 500 ms calling mqttClient.poll()
// for keepalive and message reception.  When notified by TaskTimeScheduler (every 15 s)
// it also calls sendHeaterData() to push a full status JSON to the broker.
// If the connection is lost it starts a 10-minute countdown before triggering a system reset.
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
                vTaskDelay(pdMS_TO_TICKS(1000));
                
                // TRIGGER HARDWARE RESET (ESP32)
                esp_restart();
            }            
        } else {
            // === IF CONNECTED (Normal Operation) ===
            
            // This is the fix: only update this while the connection is alive!
            lastConnectionTime = currentMillis; 
            
            if (alertSent) {
                Serial.println(F("[✓] CONNECTION RESTORED. Timer reset."));
                alertSent = false;
                isDegradedMode = false;
            }
        }

        // --- IF CONNECTED ---
        // Update the timestamp so the 10-minute timer starts over
        
        mqttClient.poll();

        if (notified && mqttClient.connected()) {
            sendHeaterData();
        }

    }
}


// Serialises all current system state into a JSON payload and publishes it to the
// status topic as a retained message.  Includes sensor readings, simulation temperatures,
// valve/pump positions, heater state, setpoints, manual override flag, and system health.
void sendHeaterData() {
    JsonDocument doc;
    doc["deviceId"] = "ESP"; // This allows the UI to identify the boiler
    doc["waterpumpactivation"] = g_WaterPumpSpeed;
    // [BOILER MODEL] Simulated boiler water temperature — real sensor to replace this in a future release
    doc["heaterWaterTemp"] = g_heaterWaterTemp;
    doc["homeTempC"] = g_homeTemp;
    doc["dhtTempC"] = g_outdoorTemp;
    doc["dhtHumidity"] = g_outdoorHumidity;
    doc["heaterActivation"] = g_heaterPosition ? "ON" : "OFF";
    doc["waterValvePosition"] = g_waterValvePosition;
    doc["systemHealth"] = systemHealth;
    // [2-WAY COMMUNICATION] Include heater state and target Home in the outgoing status JSON
    doc["heaterState"] = heaterState;
    doc["targetHomeTemp"] = targetHomeTemp;
    doc["heaterTempSetPoint"] = heaterTempSetPoint; // [HEATER SETPOINT] Boiler water target temperature
    doc["manualOverride"] = manualOverride;         // [MANUAL OVERRIDE] true = manual, false = automatic
    // [NTC] Real temperatures from the four NTC thermistors (null when open/short circuit)
    if (isnan(g_boiler1Temp))     doc["boiler1Temp"]     = nullptr; else doc["boiler1Temp"]     = (float)g_boiler1Temp;
    if (isnan(g_boiler2Temp))     doc["boiler2Temp"]     = nullptr; else doc["boiler2Temp"]     = (float)g_boiler2Temp;
    if (isnan(g_waterOutletTemp)) doc["waterOutletTemp"] = nullptr; else doc["waterOutletTemp"] = (float)g_waterOutletTemp;
    if (isnan(g_waterInletTemp))  doc["waterInletTemp"]  = nullptr; else doc["waterInletTemp"]  = (float)g_waterInletTemp;
    
    // Serialize to String first so ArduinoMqttClient knows the exact payload size.
    // Without this, the library defaults to a 256-byte buffer and truncates larger JSON.
    String payload;
    serializeJson(doc, payload);
    mqttClient.beginMessage(statusTopic, payload.length(), true);
    mqttClient.print(payload);
    mqttClient.endMessage();
    D_PRINTLN(" + Data Sent To HIVE");
}

// MQTT message callback registered with mqttClient.onMessage().
// Parses the incoming JSON command and updates the corresponding global variable:
//   "heater"        — turns heater ON/OFF (only when manualOverride is active)
//   "temperature"   — sets targetHomeTemp, clamped to 5–30 °C
//   "manualOverride"— switches between manual UI control and automatic thermostat logic
//   "heaterSetPoint"— sets boiler water target temperature, clamped to 40–70 °C
//   "weather"       — receives real outdoor temperature and humidity from the weather API
//                     via the Node.js bridge; updates g_outdoorTemp for the boiler model
// Sends an immediate sendHeaterData() after each change so the UI reflects the new state.
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
        // [MANUAL OVERRIDE] Only apply ON/OFF command when manual override is active
        if (manualOverride) {
            const char* action = doc["action"];
            heaterState = (strcmp(action, "on") == 0);
            Serial.print("Heater is now: "); Serial.println(heaterState ? "ON" : "OFF");
            sendHeaterData();
        }
    } else if (command && strcmp(command, "temperature") == 0) {
        float value = doc["value"];

        // [SAFETY] Hardware-level clamp to enforce 5-30°C range (Defense in Depth)
        if (value < 5.0f) value = 5.0f;
        if (value > 30.0f) value = 30.0f;

        targetHomeTemp = value;
        Serial.print("New Target Home: "); Serial.println(targetHomeTemp);
        // [2-WAY COMMUNICATION] Immediate Feedback: Push status to UI as soon as variable changes
        sendHeaterData();
    } else if (command && strcmp(command, "manualOverride") == 0) {
        // [MANUAL OVERRIDE] 1 = manual UI control, 0 = automatic thermostat logic
        manualOverride = (doc["value"] == 1);
        Serial.print(F("Manual Override: ")); Serial.println(manualOverride ? "ON" : "OFF");
        sendHeaterData();
    } else if (command && strcmp(command, "heaterSetPoint") == 0) {
        // [HEATER SETPOINT] Boiler water target temperature sent from UI (40–70°C range enforced here)
        float value = doc["value"];
        if (value < 40.0f) value = 40.0f;
        if (value > 70.0f) value = 70.0f;
        heaterTempSetPoint = value;
        Serial.print(F("Heater SetPoint: ")); Serial.println(heaterTempSetPoint);
        sendHeaterData();
    } else if (command && strcmp(command, "weather") == 0) {
        g_outdoorTemp     = doc["outdoorTemp"];
        g_outdoorHumidity = doc["outdoorHumidity"];
        Serial.print(F("[WEATHER] Outdoor: ")); Serial.print(g_outdoorTemp);
        Serial.print(F("C, ")); Serial.print(g_outdoorHumidity); Serial.println(F("%"));
        sendHeaterData();
    }
}

// Performs the full cloud initialisation sequence: WiFi (with SSID fallback), IP check,
// NTP time sync, MQTT credentials, Last Will and Testament, and HiveMQ connection with
// up to 10 retry attempts.  WDT.refresh() is called at each long blocking phase so the
// hardware watchdog does not fire during the expected multi-second initialisation time.
// Returns true on successful MQTT connection, false if all attempts fail.
bool initializeCloud() {
    // [2-WAY COMMUNICATION] Registration
    mqttClient.onMessage(onMqttMessageReceived);

    // 1. WiFi Connection - Network 1
    Serial.print(F("[WIFI] Connecting to SSID: "));
    Serial.println(SECRET_SSID1);
    WiFi.begin(SECRET_SSID1, SECRET_PASS1);

    unsigned long wifiTimer = millis();
    while (WiFi.status() != WL_CONNECTED && (millis() - wifiTimer < 5000)) {
        vTaskDelay(pdMS_TO_TICKS(500));
        Serial.print(".");
    }

    // 2. Fallback to Network 2
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println(F("\n[WIFI] Trying SSID 2..."));
        WiFi.disconnect();
        vTaskDelay(pdMS_TO_TICKS(1000));
        WiFi.begin(SECRET_SSID2, SECRET_PASS2);

        wifiTimer = millis();
        while (WiFi.status() != WL_CONNECTED && (millis() - wifiTimer < 5000)) {
            vTaskDelay(pdMS_TO_TICKS(500));
            Serial.print(".");
        }
    }

    // 3. IP Verification
    int ipTimeout = 0;
    while (WiFi.localIP() == IPAddress(0, 0, 0, 0) && ipTimeout < 10) {
        vTaskDelay(pdMS_TO_TICKS(100));
        ipTimeout++;
    }

    if (WiFi.localIP() == IPAddress(0, 0, 0, 0)) {
        Serial.println(F("\n[ERROR] No IP obtained."));
        return false; 
    }

    esp_task_wdt_reset(); // WiFi phase done — kick before blocking NTP + MQTT

    // 4. Sync Time (NTP)
    // configTime() launches the SNTP client asynchronously — the clock is still at
    // epoch 0 until the first response arrives.  TLS cert validation checks the
    // cert's notBefore/notAfter against the board's current time, so we must wait
    // for a valid timestamp before attempting the TLS handshake or the cert will
    // appear to be dated in the future and verification will fail with -9984.
    // Three servers so if pool.ntp.org is slow on a given boot, the SNTP client
    // falls through to the next one automatically.
    configTime(0, 0, "pool.ntp.org", "time.google.com", "time.cloudflare.com");
    Serial.print(F("[NTP] Waiting for time sync"));
    time_t now = time(nullptr);
    int ntpRetries = 0;
    while (now < 1700000000UL && ntpRetries < 60) { // up to 30 s; any time after Nov 2023
        vTaskDelay(pdMS_TO_TICKS(500));
        now = time(nullptr);
        Serial.print(F("."));
        ntpRetries++;
    }
    if (now >= 1700000000UL) {
        Serial.print(F(" OK — Unix timestamp: "));
        Serial.println((unsigned long)now); // e.g. 1750000000 confirms valid 2025+ time
    } else {
        // Clock is still at epoch — TLS will fail with -9984 if we continue.
        Serial.print(F(" TIMEOUT — Unix timestamp: "));
        Serial.println((unsigned long)now); // will print 0 or a very small number
        Serial.println(F("[NTP] Aborting cloud init (TLS requires a valid clock)"));
        return false;
    }

    // 5. MQTT Credentials
    mqttClient.setUsernamePassword(SECRET_MQTT_USER, SECRET_MQTT_PASS);
    mqttClient.setId("ESP32_Heater_Unit_ESP");
    mqttClient.setCleanSession(true);

    // --- ADDED: LAST WILL AND TESTAMENT ---
    // This tells HiveMQ: "If I drop off, tell everyone I'm offline"
    mqttClient.beginWill(statusTopic, 1, true); // topic, QOS 1, retained
    mqttClient.print("offline");
    mqttClient.endWill();
    // ---------------------------------------
    mqttClient.setKeepAliveInterval(30000); // 30 seconds timeout between hive and arduino
    // 6. TLS — set root CA certificate so WiFiClientSecure can verify HiveMQ's identity
    wifiClient.setCACert(ROOT_CA);

    // 7. HiveMQ Connection Loop
    int attempts = 0;
    while (attempts < 10) {
        esp_task_wdt_reset(); // Kick before each blocking TLS handshake
        Serial.print(F("HiveMQ Attempt #"));
        Serial.println(attempts + 1);
        
        if (mqttClient.connect(broker, port)) {
            Serial.println(F("[SUCCESS] Connected to HiveMQ!"));
            
            // --- ADDED: ONLINE STATUS ---
            // Now that we are connected, tell everyone we are back
            mqttClient.beginMessage(statusTopic, true); // retained
            mqttClient.print("online");
            mqttClient.endMessage();
            // ----------------------------

            mqttClient.subscribe(commandTopic);
            // [WEATHER] Subscribe to the dedicated retained weather topic so the Arduino
            // always receives the latest outdoor data immediately on connect.
            mqttClient.subscribe("boilers/ESP/weather");
            esp_task_wdt_reset(); // Init fully complete — normal WDT cycle takes over
            return true;
        }
        attempts++;
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    return false;
}

// ============================================================
// MIGRATION NOTES — UNO R4 WiFi → ESP32-S3
// ============================================================
// [REMOVED] esp_task_wdt_add(NULL) from TaskCloud
//   Initially added to register TaskCloud with the ESP32 hardware WDT.
//   Removed because the systemHealth bitmask mechanism in TaskwatchdogMonitor
//   already handles task health monitoring. Having TaskCloud also registered
//   with the hardware WDT caused a conflict — after initializeCloud() fails,
//   TaskCloud enters its main loop but never calls esp_task_wdt_reset() there,
//   causing the hardware WDT to fire after 5 seconds despite the system being healthy.
//   Only TaskwatchdogMonitor interacts with the hardware WDT — that is the original design.
//
// [FIXED] sendHeaterData() now only called when mqttClient.connected()
//   Previously sendHeaterData() was called every 15s regardless of connection
//   state. This caused misleading serial output showing data being "sent"
//   even when the MQTT connection was down. Added connection check so data
//   is only published when actually connected to HiveMQ.
//
// [CHANGED] NVIC_SystemReset() → esp_restart()
//   NVIC_SystemReset() is an ARM Cortex-M specific instruction
//   that talks directly to the ARM NVIC hardware block. The
//   ESP32-S3 uses Xtensa LX7 architecture — no NVIC exists.
//   esp_restart() is the ESP-IDF equivalent: triggers a full
//   system reset via the ESP32 reset controller.
//
// [CHANGED] WDT.refresh() → esp_task_wdt_reset() (3 places)
//   Same reason as main.cpp — Renesas WDT API replaced with
//   ESP32 native watchdog API.
//
// [ADDED] wifiClient.setCACert(ROOT_CA)
//   WiFiClientSecure on ESP32 has no built-in CA store unlike the UNO R4
//   WiFiSSLClient which used a pre-loaded CA store in the WiFi module firmware.
//   ROOT_CA is the ISRG Root X1 certificate (Let's Encrypt) stored in Secrets.h.
//   This allows the ESP32 to verify that HiveMQ's TLS certificate was signed
//   by a trusted authority before establishing the encrypted connection.
//
// [CHANGED] NTP/RTC sync — RTC.begin() + WiFi.getTime() + RTCTime
//   replaced with configTime(0, 0, "pool.ntp.org")
//   RTC.h and WiFi.getTime() are both Renesas-specific. On ESP32,
//   configTime() is a single call that starts the SNTP client
//   internally. The ESP-IDF handles time sync automatically in
//   the background — no RTC object or manual epoch conversion needed.
//   First argument = UTC offset in seconds (0 = UTC).
//   Second argument = daylight saving offset in seconds (0 = none).
//   Third argument = NTP server address.
// ============================================================