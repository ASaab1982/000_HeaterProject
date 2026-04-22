#ifndef SENSORS_H
#define SENSORS_H

#include "ProjectHeater.h"

void doThermistorRead();
void doDHTRead();
void TaskThermistor(void* pv);
void TaskDHT(void* pv);

#endif