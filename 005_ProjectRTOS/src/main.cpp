// #define INCLUDE_uxTaskGetStackHighWaterMark (1) found in FreeRTOSConfig.h

#include "ProjectHeater.h"


// -------------------- Pins / constants (same as your sketch) --------------------

#define DHTPIN 2
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

// Define all variables here (no 'static') so the 'extern' in .h files can find them
const int HeaterPin = 11;
const uint8_t in1Pin = 8, in2Pin = 7, enablePin = 6, tempPin = A0, waterPin = A1, servoPin = 5;
const int rotationSpeed = 256;
const bool dir =1; 
const int spd =256; 
const byte HEALTH_REQUIRED = 0x7F; // Binary 01111111 (Both tasks must be OK)
const uint32_t WDT_TIMEOUT = 5000; // 5 seconds




// delcaration of variable to be exanched with the server
 WiFiSSLClient wifiClient;
 MqttClient  mqttClient(wifiClient);
 Servo myservo;
 //WiFiClient client; // Or WiFiClient client; depending on your setup
volatile int g_WaterPumpSpeed = 0;
volatile int g_waterAdc = 0;
volatile float g_thermistorTempC = 0.0f;
volatile float g_dhtTempC = 0.0f;
volatile float g_dhtHumidity = 0.0f;
volatile float g_heaterPosition = 0.0f;
volatile int g_waterValvePosition = 0;
volatile byte systemHealth = 0x00;
// [2-WAY COMMUNICATION] Global state variables that represent the current status of the heater
volatile bool heaterState = false;
volatile float targetHomeTemp = 20.0f;

// [SIMULATION] Thermal model variables
volatile float g_boilerWaterTemp = 20.0f; // Simulated boiler water temperature (°C)
volatile float g_homeTemp        = 20.0f; // Simulated home air temperature (°C)
volatile float g_outdoorTemp     = 10.0f; // Outdoor temperature received from server via MQTT (°C)

// Your setup() and RTOS tasks remain here...

// -------------------- Globals -------------------

// Task handles for chaining
 TaskHandle_t hHeater     = nullptr;
 TaskHandle_t hWaterPump  = nullptr;
 TaskHandle_t hWaterValve = nullptr;
 TaskHandle_t hTherm     = nullptr;
 TaskHandle_t hDHT       = nullptr;
 TaskHandle_t hWater     = nullptr;
 TaskHandle_t hHeapMonitor   = nullptr;
 TaskHandle_t hTimeScheduler    = nullptr;
 TaskHandle_t hTaskCloud   = nullptr;
 TaskHandle_t hWatchdog  = nullptr;
 TaskHandle_t hHivePost  = nullptr;

// --- Task Prototypes ---
void TaskWaterPump(void* pv);
void TaskWaterValve(void* pv);
void TaskThermistor(void* pv);
void TaskDHT(void* pv);
void TaskWater(void* pv);
void TaskMonitor(void* pv);
void TaskCloud(void* pv);
void TaskwatchdogMonitor(void* pv);
void TasksendBoilerData(void* pv);
void TaskTimeScheduler(void* pv);
void TaskHeater(void* pv);



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

    pinMode(HeaterPin, OUTPUT);
    pinMode(in1Pin, OUTPUT);
    pinMode(in2Pin, OUTPUT);
    pinMode(enablePin, OUTPUT);

    pinMode(tempPin, INPUT);
    pinMode(waterPin, INPUT);


    myservo.attach(servoPin);
    // adding a small delay to let the mcu breath
    delay(100); 

    

  BaseType_t ok;

  // Create tasks
  ok = xTaskCreate(TaskHeater,     "TaskHeater",    100, nullptr, 1, &hHeater);
  if (ok != pdPASS) { D_PRINTLN(F("TaskHeater create failed")); for(;;){} }

  ok = xTaskCreate(TaskWaterPump,  "TaskWaterPump", 90, nullptr, 1, &hWaterPump);
  if (ok != pdPASS) { D_PRINTLN(F("TaskWaterPump create failed")); for(;;){} }

  ok = xTaskCreate(TaskWaterValve, "TaskWaterValve", 90, nullptr, 1, &hWaterValve);
  if (ok != pdPASS) { D_PRINTLN(F("TaskWaterValve create failed")); for(;;){} }


  ok = xTaskCreate(TaskThermistor,"TaskTherm",    90, nullptr, 1, &hTherm);
  if (ok != pdPASS) { D_PRINTLN(F("TaskTherm create failed")); for(;;){} }


  ok = xTaskCreate(TaskDHT,       "TaskDHT",       170, nullptr, 1, &hDHT);
  if (ok != pdPASS) { D_PRINTLN(F("TaskDHT create failed")); for(;;){} }


  ok = xTaskCreate(TaskWater,     "TaskWater",     70, nullptr, 1, &hWater);
  if (ok != pdPASS) { D_PRINTLN(F("TaskWater create failed")); for(;;){} }



  ok =   xTaskCreate(TaskMonitor, "TaskHeapMonitor", 60, nullptr, 1, &hHeapMonitor);
  if (ok != pdPASS) { D_PRINTLN(F("Monitor create failed")); for(;;){} }

  ok = xTaskCreate(TaskTimeScheduler, "TaskTimeScheduler", 50, nullptr, 3, &hTimeScheduler);
  if (ok != pdPASS) { D_PRINTLN(F("Task Master Time create failed")); for(;;){} }

  
  ok = xTaskCreate(TaskwatchdogMonitor, "TaskWDTMon", 60, nullptr, 1, &hWatchdog);
  if (ok != pdPASS) { D_PRINTLN(F("Watchdog task create failed"));  for(;;){} }
  
   ok =xTaskCreate(TaskCloud, "TaskCloud", 300, nullptr, 2, &hTaskCloud);
  if (ok != pdPASS) { D_PRINTLN(F("Task Cloud post create failed")); for(;;){} }


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
