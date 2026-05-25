// #define INCLUDE_uxTaskGetStackHighWaterMark (1) found in FreeRTOSConfig.h

/*
 * main.cpp — System entry point and task registry
 *
 * This file contains setup() which initialises all hardware peripherals (DHT sensor,
 * heater relay pin, H-bridge motor driver pins, servo) and creates all FreeRTOS tasks
 * before handing control to the scheduler.  It also owns every global variable shared
 * across tasks: sensor readings, simulation state, control setpoints, and the system
 * health bitmask used by the hardware watchdog.
 *
 * Task summary (all created here, implemented in their own .cpp files):
 *   TaskCloud          — WiFi/MQTT connection, sends telemetry, receives commands
 *   TaskHeater         — controls the heater relay and automatic thermostat logic
 *   TaskWaterActuator  — drives the water pump and servo valve to reach target home temp
 *   TaskHomeTemp       — advances the home temperature simulation model each cycle
 *   TaskHeaterTemp     — advances the boiler water temperature simulation model each cycle
 *   TaskDHT            — reads the DHT11 humidity/temperature sensor (or uses cloud data)
 *   TaskMonitor        — prints FreeRTOS stack high-water marks and free heap each cycle
 *   TaskTimeScheduler  — master tick: notifies every task at its required interval
 *   TaskwatchdogMonitor— kicks the hardware WDT only when all tasks have reported healthy
 */

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
const byte HEALTH_REQUIRED = 0x3F; // bits 0-5: WaterActuator pump, HomeTemp, HeaterTemp, DHT, Heater, WaterActuator valve
const uint32_t WDT_TIMEOUT = 5000; // 5 seconds




// delcaration of variable to be exanched with the server
 WiFiClientSecure wifiClient;
 MqttClient  mqttClient(wifiClient);
 Servo myservo;
 //WiFiClient client; // Or WiFiClient client; depending on your setup
volatile int g_WaterPumpSpeed = 0;
volatile int g_waterAdc = 0;
volatile float g_dhtTempC = 0.0f;
volatile float g_dhtHumidity = 0.0f;
volatile float g_heaterPosition = 0.0f;
volatile int g_waterValvePosition = 0;
volatile byte systemHealth = 0x00;
// [2-WAY COMMUNICATION] Global state variables that represent the current status of the heater
volatile bool heaterState = false;
volatile float targetHomeTemp = 20.0f;
// [MANUAL OVERRIDE] true = heater ON/OFF controlled manually from UI
//                   false = heaterState calculated automatically from boiler temperature vs setpoint
volatile bool manualOverride = false;

// [SIMULATION] Thermal model variables
volatile float g_heaterWaterTemp    = 20.0f;  // Simulated boiler water temperature (°C)
volatile float g_homeTemp           = 20.0f;  // Simulated home air temperature (°C)
volatile float g_outdoorTemp        = 10.0f;  // Startup default — overwritten by real Open-Meteo data via MQTT "weather" command
volatile float heaterTempSetPoint   = 40.0f;  // Startup default — boiler water target temperature (°C), controlled via UI (40–70°C)

// [WEATHER] Flag set to true when Node.js has pushed real outdoor weather data via MQTT.
// While true, the DHT task skips its physical sensor read so cloud values are not overwritten.
volatile bool g_useCloudWeather = false;

// Your setup() and RTOS tasks remain here...

// -------------------- Globals -------------------

// Task handles for chaining
 TaskHandle_t hHeater     = nullptr;
 TaskHandle_t hWaterActuator = nullptr;
 TaskHandle_t hHomeTemp   = nullptr;
 TaskHandle_t hHeaterTemp   = nullptr;
 TaskHandle_t hDHT       = nullptr;
 TaskHandle_t hHeapMonitor   = nullptr;
 TaskHandle_t hTimeScheduler    = nullptr;
 TaskHandle_t hTaskCloud   = nullptr;
 TaskHandle_t hWatchdog  = nullptr;

// --- Task Prototypes ---
void TaskWaterActuator(void* pv);
void TaskHomeTemp(void* pv);
void TaskHeaterTemp(void* pv);
void TaskDHT(void* pv);
void TaskMonitor(void* pv);
void TaskCloud(void* pv);
void TaskwatchdogMonitor(void* pv);
void TaskTimeScheduler(void* pv);
void TaskHeater(void* pv);



// -------------------- Arduino setup/loop --------------------

// Initialises hardware peripherals, creates all FreeRTOS tasks, then starts the scheduler.
// After vTaskStartScheduler() returns control never comes back to loop().
void setup() {
    Serial.begin(115200);

    // Watch dog start
    esp_task_wdt_init(5, true);  // 5 second timeout, panic (reset) on fire
    D_PRINTLN(F("[OK] Watchdog Hardware initialized (5s timeout)."));
  
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

  // Create TaskCloud first — needs large stack for TLS handshake + MQTT + ArduinoJson
  ok = xTaskCreate(TaskCloud, "TaskCloud", 8192, nullptr, 2, &hTaskCloud);
  if (ok != pdPASS) { D_PRINTLN(F("Task Cloud post create failed")); for(;;){} }

  ok = xTaskCreate(TaskHeater,     "TaskHeater",    2048, nullptr, 1, &hHeater);
  if (ok != pdPASS) { D_PRINTLN(F("TaskHeater create failed")); for(;;){} }

  ok = xTaskCreate(TaskWaterActuator, "TaskWaterActuator", 2048, nullptr, 1, &hWaterActuator);
  if (ok != pdPASS) { D_PRINTLN(F("TaskWaterActuator create failed")); for(;;){} }

  ok = xTaskCreate(TaskHomeTemp,"TaskHomeTemp", 2048, nullptr, 1, &hHomeTemp);
  if (ok != pdPASS) { D_PRINTLN(F("TaskHomeTemp create failed")); for(;;){} }

  ok = xTaskCreate(TaskHeaterTemp,"TaskHeaterTemp", 2048, nullptr, 1, &hHeaterTemp);
  if (ok != pdPASS) { D_PRINTLN(F("TaskHeaterTemp create failed")); for(;;){} }

  ok = xTaskCreate(TaskDHT,       "TaskDHT",       2048, nullptr, 1, &hDHT);
  if (ok != pdPASS) { D_PRINTLN(F("TaskDHT create failed")); for(;;){} }

  ok = xTaskCreate(TaskMonitor, "TaskHeapMonitor", 2048, nullptr, 1, &hHeapMonitor);
  if (ok != pdPASS) { D_PRINTLN(F("Monitor create failed")); for(;;){} }

  ok = xTaskCreate(TaskTimeScheduler, "TaskTimeScheduler", 2048, nullptr, 3, &hTimeScheduler);
  if (ok != pdPASS) { D_PRINTLN(F("Task Master Time create failed")); for(;;){} }

  ok = xTaskCreate(TaskwatchdogMonitor, "TaskWDTMon", 2048, nullptr, 1, &hWatchdog);
  if (ok != pdPASS) { D_PRINTLN(F("Watchdog task create failed"));  for(;;){} }


  D_PRINT(F("Free heap bytes: "));
  D_PRINTLN(xPortGetFreeHeapSize());

  // vTaskStartScheduler() is NOT called on ESP32.
  // The FreeRTOS scheduler is already running before setup() is called
  // by the Arduino-ESP32 framework. Calling it again would crash the system
  // by trying to reinitialize the system timer on a core that is already active.
}

void loop() {
  // Not used once RTOS scheduler is running
}




// Waits for a notification from TaskTimeScheduler each cycle.
// Kicks the hardware WDT only when systemHealth == HEALTH_REQUIRED (all 6 task bits set).
// If any bit is missing the kick is skipped, the hardware WDT fires, and the system resets.
// systemHealth is cleared to 0x00 at the end of every cycle regardless of outcome.
void TaskwatchdogMonitor(void *pv) {
    esp_task_wdt_add(NULL);   // Register this task with the ESP32 watchdog
    for (;;) {
        // Logic: Only refresh if the mask matches our requirements
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        if (systemHealth == HEALTH_REQUIRED) {
            esp_task_wdt_reset();     // Physical Hardware Kick
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

// ============================================================
// MIGRATION NOTES — UNO R4 WiFi → ESP32-S3
// ============================================================
// [CHANGED] WiFiSSLClient → WiFiClientSecure
//   WiFiSSLClient is the UNO R4 TLS client class (Renesas-specific).
//   WiFiClientSecure is the ESP32 equivalent for TLS connections.
//
// [CHANGED] WDT.begin(5592) → esp_task_wdt_init(5, true)
//   WDT.h is a Renesas-specific watchdog library. On ESP32 the
//   watchdog is managed via esp_task_wdt.h (ESP-IDF native API).
//   First argument = timeout in seconds (5s, same as before).
//   Second argument = true → system resets (panic) on watchdog fire.
//   Note: the newer ESP-IDF 5.x API uses esp_task_wdt_config_t struct
//   but the installed Arduino-ESP32 version uses the simpler 2-argument form.
//
// [CHANGED] WDT.refresh() → esp_task_wdt_reset()
//   Same concept — kick the watchdog to prevent a reset.
//   On ESP32 the watchdog is task-aware: esp_task_wdt_add(NULL)
//   registers TaskwatchdogMonitor with the watchdog before the
//   loop starts. Without registration, esp_task_wdt_reset()
//   would be silently ignored.
//
// [CHANGED] Stack sizes increased for all tasks
//   On the UNO R4 (Renesas RA4M1), the Arduino + FreeRTOS call
//   chains are shallow — 100 words was enough for most tasks.
//   On the ESP32, every Arduino API call goes through the ESP-IDF
//   layer (UART driver, DMA, interrupt handlers, FreeRTOS queues)
//   making the call stack significantly deeper.
//   Minimum safe stack on ESP32 = 2048 words for simple tasks.
//   TaskCloud = 8192 words — TLS handshake + MQTT + ArduinoJson
//   serialization create very deep call chains that require a
//   large stack to avoid overflow.
//
// [REMOVED] vTaskStartScheduler()
//   On the UNO R4, FreeRTOS was a separate library and the scheduler
//   had to be started manually at the end of setup().
//   On the ESP32, the Arduino-ESP32 framework starts the FreeRTOS
//   scheduler automatically before setup() is called — setup() itself
//   runs inside a FreeRTOS task. Calling vTaskStartScheduler() again
//   tries to reinitialize the system timer on a core that is already
//   active, causing an immediate crash (ESP_ERR_NOT_FOUND in port_systick.c).
// ============================================================
