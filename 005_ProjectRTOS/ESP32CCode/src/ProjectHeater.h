#ifndef PROJECT_HEATER_H
#define PROJECT_HEATER_H

#include <Arduino.h>
#include <ESP32Servo.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <Adafruit_Sensor.h>
#include <DHT.h>
#include <esp_task_wdt.h>
#include <ArduinoMqttClient.h>
#include <ArduinoJson.h>
#include "Heater.h"
#include "HeaterModel.h"   // [HEATER MODEL] Boiler water temperature simulation
#include "HomeTempModel.h" // [HOME MODEL]   House air temperature simulation
#include "Sensors.h"
#include "WaterActuator.h"
#include "Secrets.h"
#include "DebugMacros.h"
#include "HeapMonitor.h"
#include "DataToCloud.h"
#include "Scheduler.h"



// --- Global Pins ---
extern const int HeaterPin;
extern const uint8_t in1Pin, in2Pin, enablePin;
extern const uint8_t servoPin, tempPin, waterPin;
extern const int rotationSpeed;

// --- Global Objects & Variables ---
// Class object
extern DHT dht;
extern Servo myservo;
// extern WiFiClient client;
extern WiFiClientSecure wifiClient;
extern MqttClient  mqttClient;

// Normal object
extern float dht_h, dht_t;
extern volatile int g_WaterPumpSpeed;
extern volatile int g_waterAdc;
extern volatile float g_dhtTempC;
extern volatile float g_dhtHumidity;
extern volatile float g_heaterPosition;
extern volatile int g_waterValvePosition;
extern volatile byte systemHealth;
// [2-WAY COMMUNICATION] Extern declarations for global heater variables
extern volatile bool heaterState;
extern volatile float targetHomeTemp;
// [MANUAL OVERRIDE] true = manual UI control, false = automatic thermostat logic in Heater.cpp
extern volatile bool manualOverride;

// [SIMULATION] Thermal model variables
extern volatile float g_heaterWaterTemp;    // Simulated boiler water temperature (°C)
extern volatile float g_homeTemp;           // Simulated home air temperature (°C)
extern volatile float g_BoilerTemp;           // Simulated home air temperature (°C)
extern volatile float g_outdoorTemp;        // Outdoor temperature — overwritten by Open-Meteo data via MQTT (°C)
extern volatile float heaterTempSetPoint;   // Boiler water target temperature (°C), controlled via UI (40–70°C)

// [WEATHER] True once Node.js has sent real outdoor weather data from Open-Meteo.
// Used by Sensors.cpp to skip the physical DHT read when cloud data is available.
extern volatile bool g_useCloudWeather;
extern TaskHandle_t hHeater, hWaterActuator, hHomeTemp, hHeaterTemp, hDHT,
                     hHeapMonitor, hTimeScheduler,
                     hWatchdog, hTaskCloud;
// --- Function declaration ---

void TaskwatchdogMonitor(void* pv);
// [2-WAY COMMUNICATION] Function prototype for the incoming message handler
void onMqttMessageReceived(int messageSize);
void TaskTimeScheduler(void* pv);

extern const bool dir;
extern const int spd;

// ============================================================
// MIGRATION NOTES — UNO R4 WiFi → ESP32-S3
// ============================================================
// [ADDED]   #include <Arduino.h>
//   PlatformIO does not add this automatically. Required on ESP32
//   to access setup(), loop(), Serial, pinMode(), delay(), etc.
//
// [REMOVED] #include <Arduino_FreeRTOS.h>
//   On the UNO R4, FreeRTOS was a separate library. On the ESP32,
//   FreeRTOS is built into the framework and always available.
//   Including it manually would cause a compilation conflict.
//
// [CHANGED] #include <Servo.h> → #include <ESP32Servo.h>
//   The standard Servo library uses AVR/ARM hardware timers that
//   do not exist on the ESP32. ESP32Servo uses the ESP32 LEDC
//   timer system. The API is identical (attach, write, read).
//
// [CHANGED] #include <WiFiS3.h> → #include <WiFi.h>
//   WiFiS3.h is specific to the UNO R4 Renesas WiFi module.
//   On ESP32 the WiFi library is WiFi.h.
//
// [ADDED]   #include <WiFiClientSecure.h>
//   Required for TLS (HTTPS/MQTTS) on ESP32. Replaces the
//   WiFiSSLClient class from the UNO R4.
//
// [REMOVED] #include <WDT.h>
//   Renesas-specific watchdog library. On ESP32 the watchdog is
//   managed via esp_task_wdt.h (see main.cpp for the new API).
//
// [CHANGED] #include <RTC.h> → removed
//   The UNO R4 RTC library is Renesas-specific. On ESP32, NTP
//   time sync is handled via configTime() in DataToCloud.cpp.
//   No RTC library is needed.
//
// [CHANGED] WiFiSSLClient wifiClient → WiFiClientSecure wifiClient
//   WiFiSSLClient was the UNO R4 TLS client class. The ESP32
//   equivalent is WiFiClientSecure.
// ============================================================

#endif