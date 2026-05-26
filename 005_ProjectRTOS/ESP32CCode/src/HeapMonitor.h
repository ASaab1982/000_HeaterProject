#ifndef MONITOR_H
#define MONITOR_H

#include "ProjectHeater.h"

// Declare the function so main.ino can see it
void TaskMonitor(void* pv);
void printRtosStats();

// We use 'extern' here so the compiler knows these handles 
// are defined in the main .ino file


#endif