#ifndef WATER_PUMP_H
#define WATER_PUMP_H

#include "ProjectHeater.h"

void driveWaterPump(bool dir, int spd);
void TaskWaterPump(void* pv);

#endif
