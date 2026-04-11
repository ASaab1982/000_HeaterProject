#ifndef DC_MOTOR_H
#define DC_MOTOR_H
#include "ProjectHeater.h"


extern const uint8_t in1Pin;
extern const uint8_t in2Pin;
extern const uint8_t enablePin; // Make sure this matches your .ino name!

void driveDCMotor(bool dir, int spd);

#endif
