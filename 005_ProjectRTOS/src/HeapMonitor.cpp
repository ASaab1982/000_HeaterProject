#include "HeapMonitor.h"
#include "ProjectHeater.h" // To access your D_PRINT macros

 void TaskMonitor(void *pv) {
  (void)pv;
  for (;;) {
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    printRtosStats();
  }
}

void printRtosStats() {
    D_PRINTLN(F("---- Task Stack HWM (words) ----"));
    
    if (hHeater)     { D_PRINT(F("Heater    : ")); D_PRINTLN(uxTaskGetStackHighWaterMark(hHeater)); }
    if (hWaterPump)  { D_PRINT(F("WaterPump : ")); D_PRINTLN(uxTaskGetStackHighWaterMark(hWaterPump)); }
    if (hWaterValve) { D_PRINT(F("WaterValve: ")); D_PRINTLN(uxTaskGetStackHighWaterMark(hWaterValve)); }
    if (hTherm)   { D_PRINT(F("Thermistor: ")); D_PRINTLN(uxTaskGetStackHighWaterMark(hTherm)); }
    if (hDHT)     { D_PRINT(F("DHT       : ")); D_PRINTLN(uxTaskGetStackHighWaterMark(hDHT)); }
    if (hWater)   { D_PRINT(F("Water     : ")); D_PRINTLN(uxTaskGetStackHighWaterMark(hWater)); }
    if (hHeapMonitor)   { D_PRINT(F("HeapMon   : ")); D_PRINTLN(uxTaskGetStackHighWaterMark(hHeapMonitor)); }
    if (hTimeScheduler) { D_PRINT(F("Scheduler : ")); D_PRINTLN(uxTaskGetStackHighWaterMark(hTimeScheduler)); }
    if (hWatchdog)      { D_PRINT(F("WatchDog  : ")); D_PRINTLN(uxTaskGetStackHighWaterMark(hWatchdog)); }
    if (hTaskCloud)       { D_PRINT(F("CloudPost   : ")); D_PRINTLN(uxTaskGetStackHighWaterMark(hTaskCloud)); }

    D_PRINTLN(F("---- RTOS Stats ----"));
    D_PRINT(F("FreeHeap=")); D_PRINTLN(xPortGetFreeHeapSize());
    D_PRINT(F("MinEver =" )); D_PRINTLN(xPortGetMinimumEverFreeHeapSize());
}