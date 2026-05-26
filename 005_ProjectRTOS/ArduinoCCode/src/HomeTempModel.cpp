/*
 * HomeTempModel.cpp — Home air temperature simulation
 *
 * Simulates the thermal behaviour of the home air temperature (g_homeTemp) over time.
 * Called every 250 ms by TaskHomeTemp via doHomeTempRead().
 *
 * Two simultaneous heat exchanges are applied each step:
 *
 *   Heat gain from boiler (only when pump is running, g_WaterPumpSpeed > 0):
 *     Proportional to (g_heaterWaterTemp − g_homeTemp) × gainFactor
 *     where gainFactor scales linearly with valve position (0° = 10%, 180° = 100%)
 *     Rate constant HEAT_GAIN_RATE = 1 / 900  (home reaches boiler temp in ~15 min fully open)
 *
 *   Heat exchange with outdoors (Newton's law of cooling — bidirectional):
 *     Proportional to (g_homeTemp − g_outdoorTemp)
 *     Rate constant HEAT_LOSS_RATE = 1 / 86400  (home equilibrates with outdoors in ~24 h)
 *     Positive value = home loses heat; negative value = home gains heat from outdoors.
 *
 * Constants HEAT_GAIN_RATE and HEAT_LOSS_RATE are defined in HomeTempModel.h and shared
 * with WaterActuator.cpp for the equilibrium valve position calculation.
 */

#include "HomeTempModel.h"

// Advances g_homeTemp by one real-time step, applying boiler heat gain and outdoor exchange.
// First call only initialises the timer and returns without changing any temperature.
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
