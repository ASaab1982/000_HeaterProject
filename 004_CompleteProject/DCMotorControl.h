#ifndef DC_MOTOR_CONTROL_H
#define DC_MOTOR_CONTROL_H

#include <Arduino.h>

void driveMotor(uint8_t in1Pin, uint8_t in2Pin, uint8_t enablePin, bool dir, int spd);

#endif
