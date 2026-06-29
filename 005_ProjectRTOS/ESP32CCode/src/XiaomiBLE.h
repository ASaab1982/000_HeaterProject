#ifndef XIAOMI_BLE_H
#define XIAOMI_BLE_H

#include <Arduino.h>
#include <NimBLEDevice.h>

// MAC address of your Xiaomi Mi Temperature and Humidity Monitor 2.
// Find it in the Mi Home app → device info page.
// Format: colon-separated, lowercase  e.g. "a4:c1:38:aa:bb:cc"
#define XIAOMI_SENSOR_MAC  "a4:c1:38:57:b9:f1"

// Humidity kept as a separate global — useful for cloud telemetry
extern volatile float g_xiaomiHumidity;
extern volatile bool  g_xiaomiValid;    // true after first successful Xiaomi BLE read

void TaskXiaomiBLE(void* pv);

#endif
