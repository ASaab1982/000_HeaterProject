// Connect the port of the stepper motor driver 
int outPorts[] = {13, 12, 11, 10}; 
 
void setup() { 
  // set pins to output 
  for (int i = 0; i < 4; i++) { 
    pinMode(outPorts[i], OUTPUT); 
  } 
} 
 
void loop() 
{ 
  // Rotate a full turn 
  moveSteps(false, 2 * 64, 20); 
  delay(1000); 
  // Rotate a full turn towards another direction 
  moveSteps(true, 2 * 64, 20); 
  delay(1000); 
} 
 
void moveSteps(bool dir, int steps, unsigned int ms) { 
  for (int i = 0; i < steps; i++) { 
    moveOneStep(dir); // Rotate a step 
    delay(ms);        // Control the speed 
  } 
} 
 
void moveOneStep(bool dir) { 
  // Define a variable, use four low bit to indicate the state of port 
  static byte out = 0x01; 
  // Decide the shift direction according to the rotation direction 
  if (dir) {  // ring shift left 
    out != 0x08 ? out = out << 1 : out = 0x01; 
  } 
  else {      // ring shift right 
    out != 0x01 ? out = out >> 1 : out = 0x08; 
  } 
  // Output singal to each port 
  for (int i = 0; i < 4; i++) {
      digitalWrite(outPorts[i], (out & (0x01 << i)) ? HIGH : LOW); 
  } 
} 