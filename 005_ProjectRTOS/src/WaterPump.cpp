#include "WaterPump.h"

void TaskWaterPump(void* pv) {
  (void)pv;
  for (;;) {
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    driveWaterPump(dir, spd);
    D_PRINT(millis());
    D_PRINTLN(F(" : WaterPump running"));
  }
}

void driveWaterPump(bool dir, int spd) {
  // dir is always 1 — pump has only one direction
  // spd = 100 → pump ON | spd = 0 → pump OFF
  if (spd > 0) {
    digitalWrite(in1Pin, HIGH);
    digitalWrite(in2Pin, LOW);
    analogWrite(enablePin, 255); // Full power
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
