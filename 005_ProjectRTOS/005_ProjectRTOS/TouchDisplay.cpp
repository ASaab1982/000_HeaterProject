#include "TouchDisplay.h"

void doTouchDisplay() {
  if (touched) {
    Serial.println(F("Touch Detected!"));
    touched = false;
  }
}