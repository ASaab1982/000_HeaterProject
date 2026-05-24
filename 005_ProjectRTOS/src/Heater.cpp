#include "Heater.h"

// --- TaskHeater ---
// Waits for a notification from the scheduler every 250ms.
// Controls the heater relay based on the global heaterState variable.
void TaskHeater(void* pv) {
  (void)pv;
  for (;;) {
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    doHeaterSequence();
    D_PRINT(millis());
    D_PRINT(F(" : Heater relay -> "));
    D_PRINTLN(heaterState ? F("ON") : F("OFF"));
  }
}

// --- doHeaterSequence ---
// Controls outPorts[0] (pin 11) as a relay:
// heaterState = true  → pin HIGH → relay ON  → heater running
// heaterState = false → pin LOW  → relay OFF → heater stopped
void doHeaterSequence() {
  if (heaterState) {
    digitalWrite(HeaterPin, HIGH);
    g_heaterPosition = 1; // 1 = ON
  } else {
    digitalWrite(HeaterPin, LOW);
    g_heaterPosition = 0; // 0 = OFF
  }
  // [MANUAL OVERRIDE] When override is OFF, calculate heaterState automatically.
  // Hysteresis: ON below setpoint, OFF above setpoint+5°C, no change in between.
  if (!manualOverride) {
      if (g_heaterWaterTemp < heaterTempSetPoint)
          heaterState = true;
      else if (g_heaterWaterTemp > heaterTempSetPoint + 5.0f)
          heaterState = false;
  }

  systemHealth |= (1 << 4);
}
