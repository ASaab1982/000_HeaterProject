#include "DCMotorControl.h"

void driveMotor(uint8_t in1Pin, uint8_t in2Pin, uint8_t enablePin, bool dir, int spd) {
  if (dir) {
    digitalWrite(in1Pin, HIGH);
    digitalWrite(in2Pin, LOW);
  } else {
    digitalWrite(in1Pin, LOW);
    digitalWrite(in2Pin, HIGH);
  }

  analogWrite(enablePin, constrain(spd, 0, 255));
  if (spd > 0) {
    Serial.println(" DC moved");
  }
}
