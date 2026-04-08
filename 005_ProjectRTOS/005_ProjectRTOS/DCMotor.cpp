#include "DCMotor.h"

void driveDCMotor(bool dir, int spd) {
  if (dir) {
    digitalWrite(in1Pin, HIGH);
    digitalWrite(in2Pin, LOW);
  } else {
    digitalWrite(in1Pin, LOW);
    digitalWrite(in2Pin, HIGH);
  }

  analogWrite(enablePin, map(spd, 0,512, 0, 255));
  if (spd > 0) {
    Serial.println(" DC moved");
  }
}
