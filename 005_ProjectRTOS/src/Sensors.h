#ifndef SENSORS_H
#define SENSORS_H

#include "ProjectHeater.h"

void doHouseTempRead();
void doDHTRead();
void TaskHouseTemp(void* pv);
void TaskDHT(void* pv);

#endif