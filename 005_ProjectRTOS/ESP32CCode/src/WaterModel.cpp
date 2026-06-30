/*
 * WaterModel.cpp — Home air temperature source selection
 *
 * Two modes selected at runtime by g_usePhysicalModel:
 *
 *   false (Option 1 — Sensor mode):
 *       g_homeTemp is left as written by XiaomiBLE. No action taken.
 *
 *   true (Option 2 — Physics model):
 *       On the first valid Xiaomi reading the model is seeded from g_homeTemp.
 *       Subsequent calls advance the model by the real elapsed time (millis delta):
 *
 *         dT/dt = GAIN_RATE * (g_heaterWaterTemp − T_home) * pumpOn * valveOpen
 *               − LOSS_RATE * (T_home − g_outdoorTemp)
 *
 *       LOSS_RATE : 0.000694 °C/min/°C → heat lost to outdoors per °C of indoor/outdoor delta
 *       GAIN_RATE : 0.066667 °C/min/°C → heat gained from boiler per °C of boiler/indoor delta
 *                   at full valve opening (180°); proportional to valve state (open/closed).
 *       Both constants match test_hometempmodel.html.
 *
 *       Switching back to Option 1 resets modelInitialized so the model re-seeds
 *       correctly if Option 2 is selected again later.
 */

#include "ProjectHeater.h"
#include "WaterModel.h"
#include "XiaomiBLE.h"
#include <math.h>

// Physics constants — 50× slower than test_hometempmodel.html to match real house thermal inertia
static constexpr float LOSS_RATE = 0.00001344f;   // °C/s per °C of (indoor − outdoor) — at -20°C outdoor loss is 10% > boiler gain + additional 10%
static constexpr float GAIN_RATE = 0.066667f / 60.0f / 50.0f;   // °C/s per °C of (boiler − indoor) at full valve

static bool     modelInitialized = false;
static float    modelHomeTemp    = 0.0f;
static uint32_t lastCallMs       = 0;

void updateWaterModel() {
    if (!g_xiaomiValid) return; // wait for first real Xiaomi reading before doing anything

    if (!g_usePhysicalModel) {
        // Option 1: Xiaomi sensor owns g_homeTemp — nothing to do
        modelInitialized = false; // re-seed if switched to model later
        return;
    }

    // Option 2: physics model — seed from Xiaomi sensor on first valid read
    if (!modelInitialized) {
        if (isnan(g_homeTemp)) return; // wait for first valid Xiaomi reading
        modelHomeTemp    = g_homeTemp;
        modelInitialized = true;
        lastCallMs       = millis();
        return;
    }

    uint32_t now = millis();
    float dt = (now - lastCallMs) / 1000.0f;
    lastCallMs = now;

    bool  pumpOn   = (g_WaterPumpSpeed > 0);
    float valveOpen = g_waterValvePosition / 100.0f;  // 0.0 to 1.0

    float heatGain = 0.0f;
    if (pumpOn && valveOpen > 0.0f)
        heatGain = GAIN_RATE * (g_heaterWaterTemp - modelHomeTemp) * valveOpen;

    float heatLoss = LOSS_RATE * (modelHomeTemp - g_outdoorTemp);

    modelHomeTemp += (heatGain - heatLoss) * dt;

    g_homeTemp = modelHomeTemp;
}
