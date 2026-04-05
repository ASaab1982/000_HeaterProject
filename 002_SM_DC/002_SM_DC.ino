// Connect the port of the stepper motor driver 
int outPorts[] = {13, 12, 11, 10}; 
int in1Pin = 8;      // Define L293D channel 1 pin 
int in2Pin = 7;       // Define L293D channel 2 pin 
int enable1Pin = 6;  // Define L293D enable 1 pin 

boolean rotationDir = 0;  // Define a variable to save the motor's rotation direction, true and false are represented by positive rotation and reverse rotation. 
int rotationSpeed = 128;    // Define a variable to save the motor rotation speed 
 
 
void setup() { 
  // set pins to output 
  for (int i = 0; i < 4; i++) { 
    pinMode(outPorts[i], OUTPUT); 
  } 
  // Initialize the pin into an output mode: 
  pinMode(in1Pin, OUTPUT); 
  pinMode(in2Pin, OUTPUT); 
  pinMode(enable1Pin, OUTPUT); 
} 
 
void loop() 
{ 
  // Rotate a full turn 
  moveSteps(false, 2 * 64, 20); 
  delay(1000); 
  // Rotate a full turn towards another direction 
  moveSteps(true, 2 * 64, 20); 
  delay(1000); 
  driveMotor(rotationDir,map(rotationSpeed, 0, 512, 0, 255)); 
  rotationDir = !rotationDir;
  delay(2000);
  driveMotor(rotationDir,map(0, 0, 512, 0, 255)); 
} 


// from here down we will put all the functions 

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

void driveMotor(boolean dir, int spd) { 
  // Control motor rotation direction 
  if (rotationDir) { 
    digitalWrite(in1Pin, HIGH); 
    digitalWrite(in2Pin, LOW); 
  } 
 else { 
    digitalWrite(in1Pin, LOW); 
    digitalWrite(in2Pin, HIGH); 
  } 
  // Control motor rotation speed  
  analogWrite(enable1Pin, constrain(spd, 0, 255)); 
} 