#ifndef STEPPER_CONTROL_H
#define STEPPER_CONTROL_H

#include <Arduino.h>

void moveSteps(const int outPorts[4], bool dir, int steps, unsigned int ms);
void moveOneStep(const int outPorts[4], bool dir);

#endif
