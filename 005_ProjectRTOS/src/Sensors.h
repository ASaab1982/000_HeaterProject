#ifndef SENSORS_H
#define SENSORS_H

#include "ProjectHeater.h"

void doBoilerTempRead();
void doHomeTempRead();
void doDHTRead();
void TaskBoilerTemp(void* pv);
void TaskHomeTemp(void* pv);
void TaskDHT(void* pv);

#endif