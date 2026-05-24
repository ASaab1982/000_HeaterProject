#include "HomeTempModel.h"

void updateHomeTempModel() {
    static unsigned long lastCallMs = 0;

    unsigned long now = millis();
    if (lastCallMs == 0) {
        lastCallMs = now;
        return;
    }

    float deltaS = (now - lastCallMs) / 1000.0f;
    lastCallMs = now;

    // Heat gain from boiler water — only when pump is running
    // Valve position scales gain: 0° = 10%, 180° = 100%
    if (g_WaterPumpSpeed > 0) {
        float gainFactor = 0.10f + (g_waterValvePosition / 180.0f) * 0.90f;
        float gain = HEAT_GAIN_RATE * gainFactor * (g_heaterWaterTemp - g_homeTemp) * deltaS;
        if (gain > 0) g_homeTemp += gain;
    }

    // Heat exchange with outdoors (Newton's law — symmetric: warms up if outdoor > home, cools if outdoor < home)
    float exchange = HEAT_LOSS_RATE * (g_homeTemp - g_outdoorTemp) * deltaS;
    g_homeTemp -= exchange;
}
