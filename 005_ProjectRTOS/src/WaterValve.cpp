#include "WaterValve.h"

void TaskWaterValve(void* pv) {
  (void)pv;
  for (;;) {
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    doWaterValveSequence();
    D_PRINT(millis());
    D_PRINTLN(F(" : WaterValve moved"));
  }
}

void doWaterValveSequence() {
  int posservo = random(0, 181);
  int poslimit = constrain(posservo, 0, 180);
  myservo.write(poslimit);
  g_waterValvePosition = poslimit;
  systemHealth |= (1 << 5);
}
