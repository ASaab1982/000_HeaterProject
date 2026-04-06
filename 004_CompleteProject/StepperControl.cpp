#include "StepperControl.h"

void moveSteps(const int outPorts[4], bool dir, int steps, unsigned int ms) {
  for (int i = 0; i < steps; i++) {
    moveOneStep(outPorts, dir);
    delay(ms);
  }
  Serial.println(" Stepper moved");
}

void moveOneStep(const int outPorts[4], bool dir) {
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
