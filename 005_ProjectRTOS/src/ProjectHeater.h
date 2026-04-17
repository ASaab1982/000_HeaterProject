#ifndef PROJECT_HEATER_H
#define PROJECT_HEATER_H

#include <Arduino_FreeRTOS.h>
#include <Servo.h>
#include <WiFiS3.h>
#include <Adafruit_Sensor.h>
#include <DHT.h>
#include <WDT.h>
#include "Stepper.h"
#include "Sensors.h"
#include "DCMotor.h"
#include "ServoControl.h"
#include "MicRead.h"
#include "TouchDisplay.h"
#include "Secrets.h"
#include "DebugMacros.h"

// --- Global Pins ---
extern const int outPorts[4];
extern const uint8_t in1Pin, in2Pin, enablePin;
extern const uint8_t servoPin, touchPin, tempPin, micPin;
extern const int rotationSpeed;

// --- Global Objects & Variables ---
extern DHT dht;
extern Servo myservo;
extern WiFiClient client;
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


// --- Function Prototypes ---
void moveOneStep(bool dir);
void doStepperSequence();
void driveDCMotor(bool dir, int spd);
void doServoSequence();
void doThermistorRead();
void doDHTRead();
void doMicRead();
void doTouchDisplay();
void doTaskWebPost();


#endif