#ifndef WATER_ACTUATOR_H
#define WATER_ACTUATOR_H

#include "ProjectHeater.h"

void driveWaterPump(bool on);
void openValve();
void closeValve();
void stopValve();
void doWaterActuatorSequence();
void TaskWaterActuator(void* pv);

#endif
