#ifndef SENSORS_H
#define SENSORS_H

#include "ProjectHeater.h"

void doHeaterTempRead();
void doHomeTempRead();
void doDHTRead();
void TaskHeaterTemp(void* pv);
void TaskHomeTemp(void* pv);
void TaskDHT(void* pv);

#endif