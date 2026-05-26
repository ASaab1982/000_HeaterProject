#include <Arduino.h>

#define RGB_PIN 48  // Built-in NeoPixel on ESP32-S3-DevKitC-1

void setup() {
    Serial.begin(115200);
    Serial.println("LED test starting...");
}

void loop() {
    // Red
    neopixelWrite(RGB_PIN, 50, 0, 0);
    Serial.println("RED");
    delay(1000);

    // Blue
    neopixelWrite(RGB_PIN, 0, 0, 50);
    Serial.println("BLUE");
    delay(1000);
}
