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

void doWaterActuatorSequence() {
    // Pump control (placeholder — will be driven by house model)
    driveWaterPump(dir, spd);

    // Valve control (placeholder — will be driven by house model)
    int posservo = random(0, 181);
    int poslimit = constrain(posservo, 0, 180);
    myservo.write(poslimit);
    g_waterValvePosition = poslimit;

    systemHealth |= (1 << 5);
}
