#ifndef HEATER_FUNCTIONS_H
#define HEATER_FUNCTIONS_H

#include <Arduino.h>
#include <Servo.h>

void moveSteps(const int outPorts[4], bool dir, int steps, unsigned int ms);
void moveOneStep(const int outPorts[4], bool dir);
void moveServo(Servo& servo, int pos);
void driveMotor(uint8_t in1Pin, uint8_t in2Pin, uint8_t enablePin, bool dir, int spd);

#endif
