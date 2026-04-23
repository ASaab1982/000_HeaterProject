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
    
    if (hStepper) { D_PRINT(F("Stepper   : ")); D_PRINTLN(uxTaskGetStackHighWaterMark(hStepper)); }
    if (hDC)      { D_PRINT(F("DC        : ")); D_PRINTLN(uxTaskGetStackHighWaterMark(hDC)); }
    if (hServo)   { D_PRINT(F("Servo     : ")); D_PRINTLN(uxTaskGetStackHighWaterMark(hServo)); }
    if (hTherm)   { D_PRINT(F("Thermistor: ")); D_PRINTLN(uxTaskGetStackHighWaterMark(hTherm)); }
    if (hDHT)     { D_PRINT(F("DHT       : ")); D_PRINTLN(uxTaskGetStackHighWaterMark(hDHT)); }
    if (hMic)     { D_PRINT(F("Mic       : ")); D_PRINTLN(uxTaskGetStackHighWaterMark(hMic)); }
    if (hTouch)   { D_PRINT(F("Touch     : ")); D_PRINTLN(uxTaskGetStackHighWaterMark(hTouch)); }
    
    if (hHeapMonitor)   { D_PRINT(F("HeapMon   : ")); D_PRINTLN(uxTaskGetStackHighWaterMark(hHeapMonitor)); }
    if (hTimeScheduler) { D_PRINT(F("Scheduler : ")); D_PRINTLN(uxTaskGetStackHighWaterMark(hTimeScheduler)); }
    if (hWatchdog)      { D_PRINT(F("WatchDog  : ")); D_PRINTLN(uxTaskGetStackHighWaterMark(hWatchdog)); }
    if (hTaskCloud)       { D_PRINT(F("CloudPost   : ")); D_PRINTLN(uxTaskGetStackHighWaterMark(hTaskCloud)); }

    D_PRINTLN(F("---- RTOS Stats ----"));
    D_PRINT(F("FreeHeap=")); D_PRINTLN(xPortGetFreeHeapSize());
    D_PRINT(F("MinEver =" )); D_PRINTLN(xPortGetMinimumEverFreeHeapSize());
}