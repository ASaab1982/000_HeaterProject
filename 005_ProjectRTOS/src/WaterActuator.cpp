#include "WaterActuator.h"

void TaskWaterActuator(void* pv) {
    (void)pv;
    for (;;) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        doWaterActuatorSequence();
        D_PRINT(millis());
        D_PRINTLN(F(" : WaterActuator cycle"));
    }
}

void driveWaterPump(bool dir, int spd) {
    if (spd > 0) {
        digitalWrite(in1Pin, HIGH);
        digitalWrite(in2Pin, LOW);
        analogWrite(enablePin, 255);
        g_WaterPumpSpeed = 100;
        D_PRINTLN(F(" : WaterPump ON"));
    } else {
        digitalWrite(in1Pin, LOW);
        digitalWrite(in2Pin, LOW);
        analogWrite(enablePin, 0);
        g_WaterPumpSpeed = 0;
        D_PRINTLN(F(" : WaterPump OFF"));
    }
    systemHealth |= (1 << 0);
}

void controlHomeTemp() {
    static int currentZone = 2;  // start in equilibrium zone

    const float deadband   = 0.5f;
    const float hysteresis = 0.1f;  // half of 0.2°C — applied to each side of each boundary

    // Hysteresis thresholds:
    //   Zone1→Zone2 exit  : homeTemp > target - deadband + hysteresis  (target - 0.4°C)
    //   Zone2→Zone1 entry : homeTemp < target - deadband - hysteresis  (target - 0.6°C)
    //   Zone2→Zone3 entry : homeTemp > target + deadband + hysteresis  (target + 0.6°C)
    //   Zone3→Zone2 exit  : homeTemp < target + deadband - hysteresis  (target + 0.4°C)

    if (currentZone == 1 && g_homeTemp > targetHomeTemp - deadband + hysteresis)
        currentZone = 2;
    else if (currentZone == 3 && g_homeTemp < targetHomeTemp + deadband - hysteresis)
        currentZone = 2;
    else if (currentZone == 2 && g_homeTemp < targetHomeTemp - deadband - hysteresis)
        currentZone = 1;
    else if (currentZone == 2 && g_homeTemp > targetHomeTemp + deadband + hysteresis)
        currentZone = 3;

    if (currentZone == 1) {
        // Too cold — full heat
        driveWaterPump(true, 255);
        myservo.write(180);
        g_waterValvePosition = 180;

    } else if (currentZone == 3) {
        // Too warm — stop heating
        driveWaterPump(false, 0);
        myservo.write(0);
        g_waterValvePosition = 0;

    } else {
        // Equilibrium — valve position where gain = loss
        // Solve: HEAT_GAIN_RATE * gainFactor * (boiler - home) = HEAT_LOSS_RATE * (home - outdoor)
        float boilerDelta = g_boilerWaterTemp - g_homeTemp;
        int pos = 0;
        if (boilerDelta > 0.0f) {
            float gainFactor = (HEAT_LOSS_RATE * (g_homeTemp - g_outdoorTemp)) / (HEAT_GAIN_RATE * boilerDelta);
            float valveF = (gainFactor - 0.10f) / 0.90f * 180.0f;
            pos = (int)constrain(valveF, 0.0f, 180.0f);
        }
        driveWaterPump(true, 255);
        myservo.write(pos);
        g_waterValvePosition = pos;
    }
}

void doWaterActuatorSequence() {
    controlHomeTemp();
    systemHealth |= (1 << 5);
}
