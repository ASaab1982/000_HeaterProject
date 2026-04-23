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
#include "Stepper.h"
#include "Sensors.h"
#include "DCMotor.h"
#include "ServoControl.h"
#include "MicRead.h"
#include "TouchDisplay.h"
#include "Secrets.h"
#include "DebugMacros.h"
#include "HeapMonitor.h"
#include "DataToCloud.h"
#include "Scheduler.h"



// --- Global Pins ---
extern const int outPorts[4];
extern const uint8_t in1Pin, in2Pin, enablePin;
extern const uint8_t servoPin, touchPin, tempPin, micPin;
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
extern volatile bool touched;
extern volatile int g_dcMotorSpeed;
extern volatile int g_micAdc;
extern volatile float g_thermistorTempC;
extern volatile float g_dhtTempC;
extern volatile float g_dhtHumidity;
extern volatile float g_stepperAngleDeg;
extern volatile int g_servoPositionDeg;
extern volatile byte systemHealth;
extern TaskHandle_t hStepper, hDC, hServo, hTherm, hDHT, hMic, 
                     hTouch, hHeapMonitor, hTimeScheduler, 
                     hWatchdog, hWebPost, hTaskCloud;
// --- Function declaration ---


void TasksendBoilerData(void* pv);
void TaskwatchdogMonitor(void* pv);
void TaskTimeScheduler(void* pv);

extern const int outPorts[4];
extern const uint8_t in1Pin , in2Pin , enablePin , tempPin , micPin , touchPin , servoPin;
extern const int rotationSpeed;
extern const bool dir; 
extern const int spd; 

#endif