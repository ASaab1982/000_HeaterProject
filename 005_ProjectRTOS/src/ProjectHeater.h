#ifndef PROJECT_HEATER_H
#define PROJECT_HEATER_H

#include <Arduino_FreeRTOS.h>
#include <Servo.h>
#include <WiFiS3.h>
#include <Adafruit_Sensor.h>
#include <DHT.h>
#include <WDT.h>
#include <ArduinoMqttClient.h>
#include <ArduinoJson.h>
#include <RTC.h>
#include "Heater.h"
#include "HeaterModel.h" // [HEATER MODEL] Boiler water temperature simulation
#include "Sensors.h"
#include "WaterPump.h"
#include "WaterValve.h"
#include "WaterRead.h"
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
extern WiFiSSLClient wifiClient;
extern MqttClient  mqttClient;

// Normal object
extern float dht_h, dht_t;
extern volatile int g_WaterPumpSpeed;
extern volatile int g_waterAdc;
extern volatile float g_houseTempC;
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
extern volatile float g_boilerWaterTemp;    // Simulated boiler water temperature (°C)
extern volatile float g_homeTemp;           // Simulated home air temperature (°C)
extern volatile float g_outdoorTemp;        // Outdoor temperature — overwritten by Open-Meteo data via MQTT (°C)
extern volatile float heaterTempSetPoint;   // Boiler water target temperature (°C), controlled via UI (40–70°C)

// [WEATHER] True once Node.js has sent real outdoor weather data from Open-Meteo.
// Used by Sensors.cpp to skip the physical DHT read when cloud data is available.
extern volatile bool g_useCloudWeather;
extern TaskHandle_t hHeater, hWaterPump, hWaterValve, hHouseTemp, hDHT, hWater,
                     hHeapMonitor, hTimeScheduler,
                     hWatchdog, hTaskCloud;
// --- Function declaration ---

void TasksendBoilerData(void* pv);
void TaskwatchdogMonitor(void* pv);
// [2-WAY COMMUNICATION] Function prototype for the incoming message handler
void onMqttMessageReceived(int messageSize);
void TaskTimeScheduler(void* pv);

extern const bool dir;
extern const int spd;

#endif