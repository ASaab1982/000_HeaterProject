#ifndef PROJECT_HEATER_H
#define PROJECT_HEATER_H

#include <Arduino_FreeRTOS.h>
#include <Servo.h>
#include <WiFiS3.h>
#include <Adafruit_Sensor.h>
#include <DHT.h>
#include "Stepper.h"
#include "Sensors.h"
#include "DCMotor.h"
#include "Servo.h"

// --- Global Pins ---
extern const int outPorts[4];
extern const uint8_t in1Pin, in2Pin, enable1Pin;
extern const uint8_t servoPin, touchPin, tempPin, micPin;
extern const int rotationSpeed;

// --- Global Objects & Variables ---
extern DHT dht;
extern Servo myservo;
extern float dht_h, dht_t;
extern volatile bool touched;

// --- Function Prototypes ---
void moveOneStep(bool dir);
void doStepperSequence();
void driveDCMotor(bool dir, int spd);
void doServoSequence();
void doThermistorRead();
void doDHTRead();
void doMicRead();
void doTouchDisplay();

#endif