
#define SERVO_H

#include <Arduino.h>
#include <Servo.h>

// Tell the compiler that 'myservo' and the 'servoPin' 
// are defined in the main .ino file
extern Servo myservo;
extern const uint8_t servoPin;

// The "Menu" item for other files to call
void doServoSequence();

