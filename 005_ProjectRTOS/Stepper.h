#ifndef STEPPER_H
#define STEPPER_H
#include <Arduino.h>

extern const int outPorts[4]; // Promised to exist in main .ino
void moveOneStep(bool dir);
void doStepperSequence();

#endif