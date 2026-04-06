#include "HeaterFunctions.h"

void moveSteps(const int outPorts[4], bool dir, int steps, unsigned int ms) {
  for (int i = 0; i < steps; i++) {
    moveOneStep(outPorts, dir);
    delay(ms);
  }
}

void moveServo(Servo& servo, int pos) {
  int poslimit = constrain(pos, 0, 180);
  servo.write(poslimit);
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

void driveMotor(uint8_t in1Pin, uint8_t in2Pin, uint8_t enablePin, bool dir, int spd) {
  if (dir) {
    digitalWrite(in1Pin, HIGH);
    digitalWrite(in2Pin, LOW);
  } else {
    digitalWrite(in1Pin, LOW);
    digitalWrite(in2Pin, HIGH);
  }

  analogWrite(enablePin, constrain(spd, 0, 255));
}
