#include "WaterRead.h"

void TaskWater(void* pv) {
  (void)pv;
  for (;;) {
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    D_PRINT(millis());
    doWaterRead();
  }
}

void doWaterRead() {
  int sensorValue = analogRead(waterPin);
  g_waterAdc = sensorValue;
  D_PRINT(F(" : Water sensor: "));
  D_PRINTLN(sensorValue);
  systemHealth |= (1 << 1);
}
