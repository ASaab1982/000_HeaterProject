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
 *   TaskXiaomiBLE      — reads home temperature from Xiaomi Mi 2 BLE sensor
 *   TaskMonitor        — prints FreeRTOS stack high-water marks and free heap each cycle
 *   TaskTimeScheduler  — master tick: notifies every task at its required interval
 *   TaskwatchdogMonitor— kicks the hardware WDT only when all tasks have reported healthy
 */

#include "ProjectHeater.h"


// -------------------- Pins / constants (same as your sketch) --------------------

// Define all variables here (no 'static') so the 'extern' in .h files can find them
// Relay pins
const int PIN_RELAY_HEATER   = 9;   // Heater actuator       → HeaterActuator.cpp
const int PIN_RELAY_PUMP     = 10;  // Water pump actuator   → WaterActuator.cpp
const int PIN_RELAY_VALVE_CW = 11;  // Valve clockwise       → WaterActuator.cpp
const int PIN_RELAY_VALVE_CCW = 12; // Valve counter-clockwise → WaterActuator.cpp
const byte HEALTH_REQUIRED = 0x33; // bits 0,1,4,5: WaterActuator pump, NTC, Heater, WaterActuator valve
const uint32_t WDT_TIMEOUT = 5000; // 5 seconds




// delcaration of variable to be exanched with the server
 WiFiClientSecure wifiClient;
 MqttClient  mqttClient(wifiClient);
volatile int g_WaterPumpSpeed = 0;
volatile int g_waterAdc = 0;
volatile float g_heaterPosition = 0.0f;
volatile int g_waterValvePosition = 0;
volatile byte systemHealth = 0x00;
// [2-WAY COMMUNICATION] Global state variables that represent the current status of the heater
volatile bool heaterState = false;
volatile float targetHomeTemp = 20.0f;
// [MANUAL OVERRIDE] true = heater ON/OFF controlled manually from UI
//                   false = heaterState calculated automatically from boiler temperature vs setpoint
volatile bool manualOverride = false;
// [HOME HEATING] true = home heating enabled (pump + valve active), false = both off
volatile bool g_heatingEnabled = true;
// [SAFE STATE] true = diagnostic failure — all actuators off, UI commands blocked
volatile bool g_safeState = false;
// [PI CONTROLLER] Target valve position computed by HomeTempControl PI — read by WaterActuator
volatile float g_valveTarget      = 1.0f;
volatile float g_piProportional   = 0.0f;
volatile float g_piIntegral       = 0.0f;
volatile float g_piKp             = 1.0f;
volatile float g_piKi             = 0.0001f;

// [SIMULATION] Thermal model variables
volatile float g_heaterWaterTemp    = 50.0f;  // Startup default — overwritten by NTC reads
volatile float g_homeTemp           = 50.0f;  // Startup default — overwritten by first Xiaomi BLE read
volatile float g_outdoorTemp        = 10.0f;  // Startup default — overwritten by real weather data via MQTT "weather" command
volatile float g_outdoorHumidity    = 0.0f;   // Startup default — overwritten by real weather data via MQTT "weather" command
volatile float heaterTempSetPoint   = 40.0f;  // Startup default — boiler water target temperature (°C), controlled via UI (40–70°C)

// [HEATER MODEL] false = use NTC sensor values directly (Option 1)
//                true  = use physics model seeded from sensors at startup (Option 2)
volatile bool g_usePhysicalModel = true;

// [XIAOMI BLE] Humidity from the Xiaomi Mi 2 sensor (temperature goes straight to g_homeTemp).
volatile float g_xiaomiHumidity = 0.0f;
volatile bool     g_xiaomiValid      = false;   // true after first successful Xiaomi BLE read
volatile uint32_t g_xiaomiLastReadMs = 0;       // millis() of last valid Xiaomi reading

// [NTC] Real temperatures from the four NTC thermistors (NAN until first read)
volatile float g_boiler1Temp     = NAN;
volatile float g_boiler2Temp     = NAN;
volatile float g_waterOutletTemp = NAN;
volatile float g_waterInletTemp  = NAN;

// Your setup() and RTOS tasks remain here...

// -------------------- Globals -------------------

// Task handles for chaining
 TaskHandle_t hHeater           = nullptr;
 TaskHandle_t hWaterActuator    = nullptr;
 TaskHandle_t hHomeTempControl  = nullptr;
 TaskHandle_t hDiagFunctional  = nullptr;
 TaskHandle_t hNTC           = nullptr;
 TaskHandle_t hHeapMonitor   = nullptr;
 TaskHandle_t hTimeScheduler    = nullptr;
 TaskHandle_t hTaskCloud   = nullptr;
 TaskHandle_t hWatchdog  = nullptr;
 TaskHandle_t hXiaomiBLE = nullptr;

// --- Task Prototypes ---
void TaskWaterActuator(void* pv);
void TaskHomeTempControl(void* pv);
void TaskNTC(void* pv);
void TaskMonitor(void* pv);
void TaskCloud(void* pv);
void TaskwatchdogMonitor(void* pv);
void TaskTimeScheduler(void* pv);
void TaskHeater(void* pv);
void TaskXiaomiBLE(void* pv);



// -------------------- Arduino setup/loop --------------------

// Initialises hardware peripherals, creates all FreeRTOS tasks, then starts the scheduler.
// After vTaskStartScheduler() returns control never comes back to loop().
void setup() {
    Serial.begin(115200);

    // Watch dog start
    esp_task_wdt_init(5, true);  // 5 second timeout, panic (reset) on fire
    D_PRINTLN(F("[OK] Watchdog Hardware initialized (5s timeout)."));
  
    pinMode(PIN_RELAY_HEATER,    OUTPUT); digitalWrite(PIN_RELAY_HEATER,    LOW);
    pinMode(PIN_RELAY_PUMP,      OUTPUT); digitalWrite(PIN_RELAY_PUMP,      LOW);
    pinMode(PIN_RELAY_VALVE_CW,  OUTPUT); digitalWrite(PIN_RELAY_VALVE_CW,  LOW);
    pinMode(PIN_RELAY_VALVE_CCW, OUTPUT); digitalWrite(PIN_RELAY_VALVE_CCW, LOW);

    pinMode(NTC_PIN_BOILER1,  INPUT);
    pinMode(NTC_PIN_BOILER2,  INPUT);
    pinMode(NTC_PIN_OUTLET,   INPUT);
    pinMode(NTC_PIN_INLET,    INPUT);


    delay(100);

    

  BaseType_t ok;

  // CPU 0 — radio tasks (WiFi stack and BLE stack both live on CPU 0)
  ok = xTaskCreatePinnedToCore(TaskCloud,     "TaskCloud",     8192, nullptr, 2, &hTaskCloud,  0);
  if (ok != pdPASS) { D_PRINTLN(F("TaskCloud create failed"));    for(;;){} }

  ok = xTaskCreatePinnedToCore(TaskXiaomiBLE, "TaskXiaomiBLE", 4096, nullptr, 1, &hXiaomiBLE,  0);
  if (ok != pdPASS) { D_PRINTLN(F("TaskXiaomiBLE create failed")); for(;;){} }

  // CPU 1 — deterministic control tasks (heater, valves, sensors, scheduler, watchdog)
  ok = xTaskCreatePinnedToCore(TaskHeater,        "TaskHeater",       2048, nullptr, 1, &hHeater,        1);
  if (ok != pdPASS) { D_PRINTLN(F("TaskHeater create failed"));        for(;;){} }

  ok = xTaskCreatePinnedToCore(TaskWaterActuator,  "TaskWaterActuator", 2048, nullptr, 1, &hWaterActuator,   1);
  if (ok != pdPASS) { D_PRINTLN(F("TaskWaterActuator create failed")); for(;;){} }

  ok = xTaskCreatePinnedToCore(TaskHomeTempControl,"TaskHomeTempCtrl",  2048, nullptr, 1, &hHomeTempControl, 1);
  if (ok != pdPASS) { D_PRINTLN(F("TaskHomeTempControl create failed")); for(;;){} }

  ok = xTaskCreatePinnedToCore(TaskDiagFunctional, "TaskDiagFunctional", 2048, nullptr, 2, &hDiagFunctional,  1);
  if (ok != pdPASS) { D_PRINTLN(F("TaskDiagFunctional create failed")); for(;;){} }

  ok = xTaskCreatePinnedToCore(TaskNTC,           "TaskNTC",          2048, nullptr, 1, &hNTC,            1);
  if (ok != pdPASS) { D_PRINTLN(F("TaskNTC create failed"));           for(;;){} }

  ok = xTaskCreatePinnedToCore(TaskMonitor,       "TaskHeapMonitor",  2048, nullptr, 1, &hHeapMonitor,    1);
  if (ok != pdPASS) { D_PRINTLN(F("TaskMonitor create failed"));       for(;;){} }

  ok = xTaskCreatePinnedToCore(TaskTimeScheduler, "TaskTimeScheduler",2048, nullptr, 3, &hTimeScheduler,  1);
  if (ok != pdPASS) { D_PRINTLN(F("TaskTimeScheduler create failed")); for(;;){} }

  ok = xTaskCreatePinnedToCore(TaskwatchdogMonitor,"TaskWDTMon",      2048, nullptr, 1, &hWatchdog,       1);
  if (ok != pdPASS) { D_PRINTLN(F("TaskWDTMon create failed"));        for(;;){} }


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
    vTaskDelay(pdMS_TO_TICKS(40000)); // wait for startup sequence (2×15s delay + margin)
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
