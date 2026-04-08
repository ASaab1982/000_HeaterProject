#include "Stepper.h"
#include <Arduino_FreeRTOS.h>

void moveOneStep(bool dir) {
  static byte out = 0x01;
  if (dir) {
    out = (out << 1);
    if (out > 0x08) out = 0x01;
  } else {
    out = (out >> 1);
    if (out == 0x00) out = 0x08;
  }
  for (int i = 0; i < 4; i++) {
    digitalWrite(outPorts[i], (out & (0x01 << i)) ? HIGH : LOW);
  }
}

void doStepperSequence() {
  for (int i = 0; i < (2 * 64); i++) {
    moveOneStep(false);
    vTaskDelay(pdMS_TO_TICKS(20));
  }
  vTaskDelay(pdMS_TO_TICKS(1000));
}