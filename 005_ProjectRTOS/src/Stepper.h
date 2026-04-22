#ifndef STEPPER_H
#define STEPPER_H

#include "ProjectHeater.h"

void moveOneStep(bool dir);
void doStepperSequence();
void TaskStepper(void* pv);


#endif