#include <Servo.h>
#include <WiFiS3.h>
#include <Arduino_FreeRTOS.h>

int outPorts[] = {13, 12, 11, 10};
int in1Pin = 8;
int in2Pin = 7;
int enable1Pin = 6;
int posservo;
int servoPin = 5;

const char *ssid = "Sunrise_2.4GHz_3C4FAC";
const char *password = "Mp723phn3frd";

int rotationSpeed = 256;
Servo myservo;
WiFiServer server(80);

void TaskStepper(void *pvParameters);
void TaskDC(void *pvParameters);
void TaskServo(void *pvParameters);

void moveSteps(bool dir, int steps, unsigned int ms);
void moveOneStep(bool dir);

void moveServo(int pos);

void driveMotor(boolean dir, int spd);

void setup() {
  Serial.begin(9600);

  for (int i = 0; i < 4; i++) {
    pinMode(outPorts[i], OUTPUT);
  }
  pinMode(in1Pin, OUTPUT);
  pinMode(in2Pin, OUTPUT);
  pinMode(enable1Pin, OUTPUT);
  myservo.attach(servoPin);

  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi Connected! Waiting for DHCP address...");
  while (WiFi.localIP() == IPAddress(0, 0, 0, 0)) {
    delay(10);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected.");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  server.begin();

  randomSeed(millis());

  xTaskCreate(TaskStepper, "Stepper", 512, NULL, 1, NULL);
  xTaskCreate(TaskDC, "DC", 256, NULL, 1, NULL);
  xTaskCreate(TaskServo, "Servo", 256, NULL, 1, NULL);

  vTaskStartScheduler();
}

void loop() {
  // Not used once the RTOS scheduler is running.
}

void TaskStepper(void *pvParameters) {
  (void)pvParameters;
  for (;;) {
    moveSteps(false, 2 * 64, 20);
    vTaskDelay(pdMS_TO_TICKS(1000));
    moveSteps(true, 2 * 64, 20);
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}

void TaskDC(void *pvParameters) {
  (void)pvParameters;
  bool rotationDir = false;
  for (;;) {
    driveMotor(rotationDir, map(rotationSpeed, 0, 512, 0, 255));
    rotationDir = !rotationDir;
    vTaskDelay(pdMS_TO_TICKS(1000));
    driveMotor(rotationDir, map(0, 0, 512, 0, 255));
    vTaskDelay(pdMS_TO_TICKS(500));
    driveMotor(rotationDir, map(rotationSpeed, 0, 512, 0, 255));
    rotationDir = !rotationDir;
    vTaskDelay(pdMS_TO_TICKS(1000));
    driveMotor(rotationDir, map(0, 0, 512, 0, 255));
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}

void TaskServo(void *pvParameters) {
  (void)pvParameters;
  for (;;) {
    posservo = random(0, 181);
    moveServo(posservo);
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}

void moveSteps(bool dir, int steps, unsigned int ms) {
  for (int i = 0; i < steps; i++) {
    moveOneStep(dir);
    vTaskDelay(pdMS_TO_TICKS(ms));
  }
}

void moveOneStep(bool dir) {
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

void moveServo(int pos) {
  int poslimit = constrain(pos, 0, 180);
  myservo.write(poslimit);
}


void driveMotor(boolean dir, int spd) {
  if (dir) {
    digitalWrite(in1Pin, HIGH);
    digitalWrite(in2Pin, LOW);
  } else {
    digitalWrite(in1Pin, LOW);
    digitalWrite(in2Pin, HIGH);
  }
  analogWrite(enable1Pin, constrain(spd, 0, 255));
}