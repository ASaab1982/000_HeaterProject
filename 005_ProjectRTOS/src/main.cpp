// #define INCLUDE_uxTaskGetStackHighWaterMark (1) found in FreeRTOSConfig.h

#include "ProjectHeater.h"


// -------------------- Pins / constants (same as your sketch) --------------------

#define DHTPIN 2
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

// Define all variables here (no 'static') so the 'extern' in .h files can find them
const int outPorts[4] = {13, 12, 11, 10};
const uint8_t in1Pin = 8, in2Pin = 7, enablePin = 6, tempPin = A0, micPin = A1, touchPin = 3, servoPin = 5;
const int rotationSpeed = 256;
const bool dir =1; 
const int spd =256; 
const byte HEALTH_REQUIRED = 0x7F; // Binary 01111111 (Both tasks must be OK)
const uint32_t WDT_TIMEOUT = 5000; // 5 seconds

const char broker[] = "d72dc8b632b04c8c91c4702a5b164d59.s1.eu.hivemq.cloud";
int        port     = 8883;
const char topic[]  = "boilers/B1/status"; 
bool isDegradedMode = false; // Declare this at the top of your sketch


// delcaration of variable to be exanched with the server
 WiFiSSLClient wifiClient;
 MqttClient  mqttClient(wifiClient);
 Servo myservo;
 WiFiClient client; // Or WiFiClient client; depending on your setup
volatile int g_dcMotorSpeed = 0;
volatile int g_micAdc = 0;
volatile float g_thermistorTempC = 0.0f;
volatile float g_dhtTempC = 0.0f;
volatile float g_dhtHumidity = 0.0f;
volatile float g_stepperAngleDeg = 0.0f;
volatile int g_servoPositionDeg = 0;
volatile bool touched = false;
volatile byte systemHealth = 0x00;

// Your setup() and RTOS tasks remain here...

// -------------------- Globals -------------------

// Task handles for chaining
 TaskHandle_t hStepper   = nullptr;
 TaskHandle_t hDC        = nullptr;
 TaskHandle_t hServo     = nullptr;
 TaskHandle_t hTherm     = nullptr;
 TaskHandle_t hDHT       = nullptr;
 TaskHandle_t hMic       = nullptr;
 TaskHandle_t hTouch     = nullptr;
 TaskHandle_t hHeapMonitor   = nullptr;
 TaskHandle_t hTimeScheduler    = nullptr;
 TaskHandle_t hWebPost   = nullptr;
 TaskHandle_t hWatchdog  = nullptr;
 TaskHandle_t hHivePost  = nullptr;

// --- Task Prototypes ---
void TaskDC(void* pv);
void TaskServo(void* pv);
void TaskThermistor(void* pv);
void TaskDHT(void* pv);
void TaskMic(void* pv);
void TaskTouch(void* pv);
void TaskMonitor(void* pv);
void TaskWebPost(void* pv);
void TaskwatchdogMonitor(void* pv);
void TasksendBoilerData(void* pv);
void TaskTimeScheduler(void* pv);
void TaskStepper(void* pv);


// -------------------- Touch ISR --------------------
void handleTouchInterrupt() {
  vTaskNotifyGiveFromISR(hTouch, NULL);
}


// -------------------- Arduino setup/loop --------------------
void setup() {
    Serial.begin(115200);

    // Watch dog start
    if (!WDT.begin(5592)) {
    D_PRINTLN(F("WDT.begin a échoué. Tentative de refresh forcé..."));
    WDT.refresh(); 
    D_PRINTLN(F("Si vous lisez ceci, le code continue malgré l'erreur."));
      }
    else {
    // SCÉNARIO B : SUCCÈS
    D_PRINTLN(F("[OK] Watchdog Hardware initialized (5s  timeout)."));
  }
  
  

     dht.begin();

    for (int i = 0; i < 4; i++) {
      pinMode(outPorts[i], OUTPUT);
    }

    pinMode(in1Pin, OUTPUT);
    pinMode(in2Pin, OUTPUT);
    pinMode(enablePin, OUTPUT);

    pinMode(tempPin, INPUT);
    pinMode(micPin, INPUT);

    pinMode(touchPin, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(touchPin), handleTouchInterrupt, RISING);

    myservo.attach(servoPin);
    // adding a small delay to let the mcu breath
    delay(100); 

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

  BaseType_t ok;

  // Create tasks
  ok = xTaskCreate(TaskStepper,   "TaskStepper",   90, nullptr, 1, &hStepper);
  if (ok != pdPASS) { D_PRINTLN(F("TaskStepper create failed")); for(;;){} }

  ok = xTaskCreate(TaskDC,        "TaskDC",        90, nullptr, 1, &hDC);
  if (ok != pdPASS) { D_PRINTLN(F("TaskDC create failed")); for(;;){} }


  ok =  xTaskCreate(TaskServo,     "TaskServo",     90, nullptr, 1, &hServo);
  if (ok != pdPASS) { D_PRINTLN(F("TaskServo create failed")); for(;;){} }


  ok = xTaskCreate(TaskThermistor,"TaskTherm",    90, nullptr, 1, &hTherm);
  if (ok != pdPASS) { D_PRINTLN(F("TaskTherm create failed")); for(;;){} }


  ok = xTaskCreate(TaskDHT,       "TaskDHT",       180, nullptr, 1, &hDHT);
  if (ok != pdPASS) { D_PRINTLN(F("TaskDHT create failed")); for(;;){} }


  ok = xTaskCreate(TaskMic,       "TaskMic",       70, nullptr, 1, &hMic);
  if (ok != pdPASS) { D_PRINTLN(F("TaskMic create failed")); for(;;){} }


  ok = xTaskCreate(TaskTouch,     "TaskTouch",    50, nullptr, 1, &hTouch);
  if (ok != pdPASS) { D_PRINTLN(F("TaskTouch create failed")); for(;;){} }

  ok =   xTaskCreate(TaskMonitor, "TaskHeapMonitor", 70, nullptr, 1, &hHeapMonitor);
  if (ok != pdPASS) { D_PRINTLN(F("Monitor create failed")); for(;;){} }

  ok = xTaskCreate(TaskTimeScheduler, "TaskTimeScheduler", 60, nullptr, 3, &hTimeScheduler);
  if (ok != pdPASS) { D_PRINTLN(F("Task Master Time create failed")); for(;;){} }

   ok =xTaskCreate(TaskWebPost, "TaskWeb", 300, nullptr, 1, &hWebPost);
  if (ok != pdPASS) { D_PRINTLN(F("Task web post create failed")); for(;;){} }

  ok = xTaskCreate(TaskwatchdogMonitor, "TaskWDTMon", 50, nullptr, 1, &hWatchdog);
  if (ok != pdPASS) { D_PRINTLN(F("Watchdog task create failed"));  for(;;){} }

  D_PRINT(F("Free heap bytes: "));
  D_PRINTLN(xPortGetFreeHeapSize());

  vTaskStartScheduler();
}

void loop() {
  // Not used once RTOS scheduler is running
}




void TaskwatchdogMonitor(void *pv) {
    for (;;) {
        // Logic: Only refresh if the mask matches our requirements
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        D_PRINT(F("System Health   : ")); 
        D_PRINTLN(systemHealth);
        if (systemHealth >= 0 ) { // == HEALTH_REQUIRED this is atemporary fix to make all functions work
            WDT.refresh();            // Physical Hardware Kick
            #if DEBUG
                Serial.println(F("WDT: All tasks reported. System Healthy."));
            #endif
        } else {
            #if DEBUG
                Serial.print(F("WDT Warning: Missing task report. Health Mask: "));
                Serial.println(systemHealth, BIN);
            #endif
        }
        systemHealth = 0x00;      // Reset for next cycle regardless of success
    }
}

