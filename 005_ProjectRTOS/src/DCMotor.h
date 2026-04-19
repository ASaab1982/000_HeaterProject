#ifndef DC_MOTOR_H
#define DC_MOTOR_H
#include "ProjectHeater.h"

/* aldready in ProjectHeater.h, no need to declae it 
extern const uint8_t in1Pin;
extern const uint8_t in2Pin;
extern const uint8_t enablePin; // 
*/

void driveDCMotor(bool dir, int spd);

#endif
