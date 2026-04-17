#include "TaskWebPost.h"

// Helper to calculate JSON length without creating a String object
int calculateJsonLength() {
    int len = 0;
    len += 1; // {
    len += String("\"dhtTempC\":").length() + String(g_dhtTempC).length() + 1;
    len += String("\"dhtHumidity\":").length() + String(g_dhtHumidity).length() + 1;
    len += String("\"dcMotorSpeed\":").length() + String(g_dcMotorSpeed).length() + 1;
    len += String("\"micAdc\":").length() + String(g_micAdc).length() + 1;
    len += String("\"thermistorTempC\":").length() + String(g_thermistorTempC).length() + 1;
    len += String("\"stepperAngleDeg\":").length() + String(g_stepperAngleDeg).length() + 1;
    len += String("\"servoPositionDeg\":").length() + String(g_servoPositionDeg).length() + 1;
    len += String("\"touched\":").length() + (touched ? 4 : 5); // true vs false
    // Inside calculateJsonLength()
    len += String("\"freeHeap\":").length() + String(xPortGetFreeHeapSize()).length() + 1;
    len += String("\"minHeap\":").length() + String(xPortGetMinimumEverFreeHeapSize()).length() + 1;
    len += 1; // }
    return len;
}

void doTaskWebPost() {
    if (client.connect(serverAddress, serverPort)) {
        int jsonLength = calculateJsonLength();

        // 1. Send Headers
        client.println(F("POST /api/telemetry HTTP/1.1"));
        client.print(F("Host: ")); client.println(serverAddress);
        client.print(F("Authorization: Bearer ")); client.println(apiToken);
        client.println(F("Content-Type: application/json"));
        client.print(F("Content-Length: ")); client.println(jsonLength);
        client.println(); // Header/Body separator

        // 2. Stream JSON directly (No big String object!)
        client.print(F("{"));
        client.print(F("\"dhtTempC\":"));          client.print(g_dhtTempC);          client.print(F(","));
        client.print(F("\"dhtHumidity\":"));       client.print(g_dhtHumidity);       client.print(F(","));
        client.print(F("\"dcMotorSpeed\":"));      client.print(g_dcMotorSpeed);      client.print(F(","));
        client.print(F("\"micAdc\":"));            client.print(g_micAdc);            client.print(F(","));
        client.print(F("\"thermistorTempC\":"));   client.print(g_thermistorTempC);   client.print(F(","));
        client.print(F("\"stepperAngleDeg\":"));   client.print(g_stepperAngleDeg);   client.print(F(","));
        client.print(F("\"servoPositionDeg\":"));  client.print(g_servoPositionDeg);  client.print(F(","));
        client.print(F("\"touched\":"));           client.print(touched ? F("true") : F("false"));
        client.print(F(",\"freeHeap\":"));         client.print(xPortGetFreeHeapSize());
        client.print(F(",\"minHeap\":"));          client.print(xPortGetMinimumEverFreeHeapSize());
        client.print(F("}"));

        // 3. Efficient Response Check
        unsigned long timeout = millis();
        while (client.connected() && millis() - timeout < 1000) {
            if (client.available()) {
                char c = client.read();
                // Just peek at the first line for the HTTP status
                if (c == '\n') break; 
            }
        }

        client.stop();
        D_PRINTLN(F("[+] Telemetry streamed successfully."));
    } else {
        D_PRINTLN(F("[!] Server connection failed."));
    }
    systemHealth |= (1 << 6); // Health bit for mic read is OK

}