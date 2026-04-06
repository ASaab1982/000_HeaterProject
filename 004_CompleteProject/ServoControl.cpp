#include "ServoControl.h"

void moveServo(Servo &servo, int pos) {
  int poslimit = constrain(pos, 0, 180);
  servo.write(poslimit);
  Serial.println(" Servo moved");
}
