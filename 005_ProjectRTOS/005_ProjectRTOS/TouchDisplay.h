#define TOUCH_DISPLAY_H
#include <Arduino.h>

extern volatile bool touched; // Defined in main .ino
void doTouchDisplay();