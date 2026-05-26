#ifndef WATER_ACTUATOR_H
#define WATER_ACTUATOR_H

#include "ProjectHeater.h"

void driveWaterPump(bool dir, int spd);
void controlHomeTemp();
void doWaterActuatorSequence();
void TaskWaterActuator(void* pv);

#endif
