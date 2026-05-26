#ifndef HOME_TEMP_MODEL_H
#define HOME_TEMP_MODEL_H

#include "ProjectHeater.h"

constexpr float HEAT_GAIN_RATE = 1.0f / 900.0f;    // °C/s per °C difference (boiler → home)
constexpr float HEAT_LOSS_RATE = 1.0f / 86400.0f;   // °C/s per °C difference (home ↔ outdoor)

void updateHomeTempModel();

#endif
