#include "MicRead.h"

 void TaskMic(void* pv) {
  (void)pv;
  for (;;) {
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    D_PRINT(millis()); // Show exactly when this happened
    doMicRead();
  }
}

void doMicRead() {
  int sensorValue = analogRead(micPin);
  g_micAdc = sensorValue;
  D_PRINT(F(" : Microphone: "));
  D_PRINTLN(sensorValue);
  systemHealth |= (1 << 1); // Health bit for mic read is OK

}