/*
 * Scheduler.cpp — Master task scheduler
 *
 * TaskTimeScheduler is the heartbeat of the system.  It runs at the highest task priority
 * (priority 3) and sequentially notifies every other task at a fixed 250 ms cadence using
 * xTaskNotifyGive().  Each task wakes only when notified, performs its work, then blocks
 * again — this prevents busy-waiting and keeps CPU usage minimal.
 *
 * Notification order and timing (each slot = 250 ms):
 *   Slot 1 — TaskHeater
 *   Slot 2 — TaskWaterActuator
 *   Slot 3 — TaskHeaterTemp
 *   Slot 4 — TaskHomeTemp
 *   Slot 5 — TaskDHT
 *   Slot 6 — TaskCloud (only every 15 s, not every cycle)
 *   Slot 7 — TaskMonitor (heap stats)
 *   Slot 8 — TaskwatchdogMonitor
 *
 * One full round takes 8 × 250 ms = 2 seconds.
 * Cloud telemetry is sent every 15 seconds (every ~7–8 full rounds).
 */

#include "ProjectHeater.h" // For D_PRINTLN

// FreeRTOS task — runs continuously at priority 3, notifying all other tasks in sequence.
// vTaskDelayUntil keeps the 250 ms rhythm accurate even if a notification takes non-zero time.
void TaskTimeScheduler(void* pv) {
  (void)pv;
  TickType_t xLastWakeTime = xTaskGetTickCount();
  const TickType_t xInterval = pdMS_TO_TICKS(250); 
  
  // Track when we last sent data to the cloud
  TickType_t xLastCloudTime = xTaskGetTickCount();
  const TickType_t xCloudInterval = pdMS_TO_TICKS(15000); // 15 seconds push data to the hive

  for (;;) {
    if (hHeater)      xTaskNotifyGive(hHeater);
    vTaskDelayUntil(&xLastWakeTime, xInterval);

    if (hWaterActuator) xTaskNotifyGive(hWaterActuator);
    vTaskDelayUntil(&xLastWakeTime, xInterval);

    if (hHeaterTemp)  xTaskNotifyGive(hHeaterTemp);
    vTaskDelayUntil(&xLastWakeTime, xInterval);

    if (hHomeTemp)    xTaskNotifyGive(hHomeTemp);
    vTaskDelayUntil(&xLastWakeTime, xInterval);

    if (hDHT)         xTaskNotifyGive(hDHT);
    vTaskDelayUntil(&xLastWakeTime, xInterval);

    // --- CONDITIONALLY NOTIFY CLOUD ---
    // Check if 60 seconds have passed since the last cloud update
    if (hTaskCloud && (xTaskGetTickCount() - xLastCloudTime >= xCloudInterval)) {
        xTaskNotifyGive(hTaskCloud); 
        xLastCloudTime = xTaskGetTickCount(); // Reset the timer
    }
    // We still delay here to keep the 250ms "rhythm" for the other tasks
    vTaskDelayUntil(&xLastWakeTime, xInterval);

    if (hHeapMonitor) xTaskNotifyGive(hHeapMonitor);
    vTaskDelayUntil(&xLastWakeTime, xInterval);

    if (hWatchdog)    xTaskNotifyGive(hWatchdog);
    vTaskDelayUntil(&xLastWakeTime, xInterval);

  }
}