/*
 * HeapMonitor.cpp — FreeRTOS runtime diagnostics
 *
 * TaskMonitor is a passive diagnostic task woken by TaskTimeScheduler once per cycle.
 * It prints a "Cycle Stats" block to the serial console showing:
 *   - Stack high-water mark (minimum free words) for every task — used to tune stack sizes
 *     and detect stack overflow risks before they cause crashes
 *   - Current free heap and minimum-ever free heap — monitors heap fragmentation over time
 *   - systemHealth bitmask in binary — shows which tasks reported healthy this cycle
 *
 * All output is guarded by the DEBUG macro (D_PRINT / D_PRINTLN) so it compiles out
 * completely in production builds.
 */

#include "HeapMonitor.h"
#include "ProjectHeater.h"

// FreeRTOS task — waits for a scheduler notification then calls printRtosStats().
void TaskMonitor(void *pv) {
  (void)pv;
  for (;;) {
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    printRtosStats();
  }
}

// Prints the full diagnostics block: stack high-water mark per task, free heap,
// minimum-ever free heap, and systemHealth bitmask in binary format.
void printRtosStats() {
    D_PRINTLN(F("====== Cycle Stats ======"));
    if (hHeater)        { D_PRINT(F("Heater    : ")); D_PRINTLN(uxTaskGetStackHighWaterMark(hHeater)); }
    if (hWaterActuator) { D_PRINT(F("WaterAct  : ")); D_PRINTLN(uxTaskGetStackHighWaterMark(hWaterActuator)); }
    if (hHeaterTemp)    { D_PRINT(F("HeaterTemp: ")); D_PRINTLN(uxTaskGetStackHighWaterMark(hHeaterTemp)); }
    if (hDHT)           { D_PRINT(F("DHT       : ")); D_PRINTLN(uxTaskGetStackHighWaterMark(hDHT)); }
    if (hNTC)           { D_PRINT(F("NTC       : ")); D_PRINTLN(uxTaskGetStackHighWaterMark(hNTC)); }
    if (hHeapMonitor)   { D_PRINT(F("HeapMon   : ")); D_PRINTLN(uxTaskGetStackHighWaterMark(hHeapMonitor)); }
    if (hTimeScheduler) { D_PRINT(F("Scheduler : ")); D_PRINTLN(uxTaskGetStackHighWaterMark(hTimeScheduler)); }
    if (hWatchdog)      { D_PRINT(F("WatchDog  : ")); D_PRINTLN(uxTaskGetStackHighWaterMark(hWatchdog)); }
    if (hTaskCloud)     { D_PRINT(F("CloudPost : ")); D_PRINTLN(uxTaskGetStackHighWaterMark(hTaskCloud)); }
    D_PRINT(F("FreeHeap  : ")); D_PRINTLN(xPortGetFreeHeapSize());
    D_PRINT(F("MinEver   : ")); D_PRINTLN(xPortGetMinimumEverFreeHeapSize());
    D_PRINT(F("Health    : ")); D_PRINTF(systemHealth, BIN); D_PRINTLN(F(""));
    D_PRINTLN(F("========================="));
}
