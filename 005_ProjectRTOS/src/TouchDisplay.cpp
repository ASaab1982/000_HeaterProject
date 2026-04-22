#include "TouchDisplay.h"

 void TaskTouch(void* pv) {
  (void)pv;
  for (;;) {
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    D_PRINT(millis()); // Show exactly when this happened
    doTouchDisplay();
  }
} 

void doTouchDisplay() {
    D_PRINTLN(F(" : Touch Detected!"));
}