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




// delcaration of variable to be exanched with the server
Servo myservo;
WiFiClient client;
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
static TaskHandle_t hStepper   = nullptr;
static TaskHandle_t hDC        = nullptr;
static TaskHandle_t hServo     = nullptr;
static TaskHandle_t hTherm     = nullptr;
static TaskHandle_t hDHT       = nullptr;
static TaskHandle_t hMic       = nullptr;
static TaskHandle_t hTouch     = nullptr;
static TaskHandle_t hHeapMonitor   = nullptr;
static TaskHandle_t hTimeScheduler    = nullptr;
static TaskHandle_t hWebPost   = nullptr;
static TaskHandle_t hWatchdog  = nullptr;

// --- Task Prototypes ---
static void TaskStepper(void* pv);
static void TaskDC(void* pv);
static void TaskServo(void* pv);
static void TaskThermistor(void* pv);
static void TaskDHT(void* pv);
static void TaskMic(void* pv);
static void TaskTouch(void* pv);
static void TaskMonitor(void* pv);
static void TaskTimeScheduler(void* pv);
static void TaskWebPost(void* pv);
static void TaskwatchdogMonitor(void* pv);

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

  // WiFi (kept from your setup)
  D_PRINT(F("Connecting to "));
  D_PRINTLN(WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    D_PRINT(F("."));
  }

  D_PRINTLN(F("\nWiFi Connected! Waiting for DHCP address..."));
  while (WiFi.localIP() == IPAddress(0, 0, 0, 0)) {
    delay(10);
    D_PRINT(F("."));
  }

  D_PRINTLN(F(""));
  D_PRINTLN(F("WiFi connected."));
  D_PRINTLN(F("IP address: "));
  D_PRINTLN(WiFi.localIP());
  // server.begin();

  randomSeed(millis());
 
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



// -------------------- Tasks (one per function) --------------------
static void TaskTimeScheduler(void* pv) {
  (void)pv;
  TickType_t xLastWakeTime = xTaskGetTickCount();
  // We want a 1-second gap between each task notification
  const TickType_t xOneSecond = pdMS_TO_TICKS(250); //xOneSecond is now 250 ms

  for (;;) {
    // --- Task 0 ---
    xTaskNotifyGive(hStepper);
    vTaskDelayUntil(&xLastWakeTime, xOneSecond);

    // --- Task 1 ---
    xTaskNotifyGive(hDC);
    vTaskDelayUntil(&xLastWakeTime, xOneSecond);

    // --- Task 2 ---
    xTaskNotifyGive(hServo);
    vTaskDelayUntil(&xLastWakeTime, xOneSecond);

    // --- Task 3 ---
    xTaskNotifyGive(hTherm);
    vTaskDelayUntil(&xLastWakeTime, xOneSecond);

    // --- Task 4 ---
    xTaskNotifyGive(hDHT);
    vTaskDelayUntil(&xLastWakeTime, xOneSecond);

    // --- Task 4 ---
    xTaskNotifyGive(hMic);
    vTaskDelayUntil(&xLastWakeTime, xOneSecond);

    // --- Task 5: Web 
    xTaskNotifyGive(hWebPost); 
    vTaskDelayUntil(&xLastWakeTime, xOneSecond);

    // --- Task 6 : Heap Monitor
    xTaskNotifyGive(hHeapMonitor);
    vTaskDelayUntil(&xLastWakeTime, xOneSecond);

    // --- Task 6 : What dog function
     xTaskNotifyGive(hWatchdog);
    vTaskDelayUntil(&xLastWakeTime, xOneSecond);

    // the full cycle is now 2 seconds and each function start after 250 ms

    D_PRINTLN(F("--- Cycle Complete ---"));
  }
}

static void printRtosStats() {
    D_PRINTLN("---- Task Stack HWM (words) ----");
  if (hStepper)         { D_PRINT(F("Stepper   : "));     D_PRINTLN(uxTaskGetStackHighWaterMark(hStepper)); }
  if (hDC)              { D_PRINT(F("DC        : "));     D_PRINTLN(uxTaskGetStackHighWaterMark(hDC)); }
  if (hServo)           { D_PRINT(F("Servo     : "));     D_PRINTLN(uxTaskGetStackHighWaterMark(hServo)); }
  if (hTherm)           { D_PRINT(F("Thermistor: "));     D_PRINTLN(uxTaskGetStackHighWaterMark(hTherm)); }
  if (hDHT)             { D_PRINT(F("DHT       : "));     D_PRINTLN(uxTaskGetStackHighWaterMark(hDHT)); }
  if (hMic)             { D_PRINT(F("Mic       : "));     D_PRINTLN(uxTaskGetStackHighWaterMark(hMic)); }
  if (hTouch)           { D_PRINT(F("Touch     : "));     D_PRINTLN(uxTaskGetStackHighWaterMark(hTouch)); }
  if (hHeapMonitor)     { D_PRINT(F("HeapMonitor   : ")); D_PRINTLN(uxTaskGetStackHighWaterMark(hHeapMonitor)); }
  if (hTimeScheduler)   { D_PRINT(F("TimeScedhuler  :")); D_PRINTLN(uxTaskGetStackHighWaterMark(hTimeScheduler)); }
  if (hWatchdog)        { D_PRINT(F("WatchDog   : "));    D_PRINTLN(uxTaskGetStackHighWaterMark(hWatchdog)); }
  if (hWebPost)         { D_PRINT(F("WebPost   : "));     D_PRINTLN(uxTaskGetStackHighWaterMark(hWebPost)); }
  

  D_PRINTLN("---- RTOS Stats ----");
  D_PRINT(F("FreeHeap=")); D_PRINTLN(xPortGetFreeHeapSize());
  D_PRINT(F("MinEver =" )); D_PRINTLN(xPortGetMinimumEverFreeHeapSize());
}

static void TaskwatchdogMonitor(void *pv) {
    for (;;) {
        // Logic: Only refresh if the mask matches our requirements
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        D_PRINT(F("System Health   : ")); 
        D_PRINTLN(systemHealth);
        if (systemHealth == HEALTH_REQUIRED) {
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

static void TaskStepper(void* pv) {
  (void)pv;
  for (;;) {
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    doStepperSequence();
    D_PRINT(millis()); // Show exactly when this happened
    D_PRINTLN(F(" : Stepper moved"));
  }
}

static void TaskDC(void* pv) {
  (void)pv;
  for (;;) {
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    driveDCMotor(dir, spd);
    D_PRINT(millis()); // Show exactly when this happened
    D_PRINTLN(F(" : DC moved"));
  }
}

static void TaskServo(void* pv) {
  (void)pv;
  for (;;) {
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    doServoSequence();
    D_PRINT(millis()); // Show exactly when this happened
    D_PRINTLN(F(" : Servo moved"));
  }
}

static void TaskThermistor(void* pv) {
  (void)pv;
  for (;;) {
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    D_PRINT(millis()); // Show exactly when this happened
    doThermistorRead();  }
}

static void TaskDHT(void* pv) {
  (void)pv;
  for (;;) {
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    D_PRINT(millis()); // Show exactly when this happened
    doDHTRead();
  }
}

static void TaskMic(void* pv) {
  (void)pv;
  for (;;) {
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    D_PRINT(millis()); // Show exactly when this happened
    doMicRead();
  }
}

static void TaskTouch(void* pv) {
  (void)pv;
  for (;;) {
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    D_PRINT(millis()); // Show exactly when this happened
    doTouchDisplay();
  }
} 

static void TaskWebPost(void *pv) {
  (void)pv;
  for (;;) {
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    doTaskWebPost();
  }
}

static void TaskMonitor(void *pv) {
  (void)pv;
  for (;;) {
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    printRtosStats();
  }
}
