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
 *   Slot 3 — TaskNTC  (4× NTC thermistors)
 *   Slot 4 — TaskCloud (only every 5 min, not every cycle)
 *   Slot 5 — TaskMonitor (heap stats)
 *   Slot 6 — TaskwatchdogMonitor
 *   + TaskXiaomiBLE notified once, exactly 2 min after each cloud push
 *
 * One full round takes 6 × 250 ms = 1.5 seconds.
 * Cloud telemetry is sent every 5 min.  The Xiaomi BLE read is triggered 2 min after
 * each cloud push so the WiFi radio has finished its MQTT transaction before BLE
 * coexistence begins (both share the 2.4 GHz antenna on the ESP32-S3).
 */

#include "ProjectHeater.h"

// FreeRTOS task — runs continuously at priority 3, notifying all other tasks in sequence.
// vTaskDelayUntil keeps the 250 ms rhythm accurate even if a notification takes non-zero time.
void TaskTimeScheduler(void* pv) {
  (void)pv;
  TickType_t xLastWakeTime = xTaskGetTickCount();
  const TickType_t xInterval = pdMS_TO_TICKS(250);

  TickType_t xLastCloudTime  = xTaskGetTickCount();
  TickType_t xLastXiaomiTime = xTaskGetTickCount();
  const TickType_t xCloudInterval  = pdMS_TO_TICKS(30000);   // 30 seconds
  const TickType_t xXiaomiInterval = pdMS_TO_TICKS(300000);  // 5 minutes

  // Trigger the first Xiaomi BLE read immediately at startup so g_homeTemp is seeded
  // before any control loop uses it — without this it would wait 5 min (cloud) + 2 min (offset).
  if (hXiaomiBLE) xTaskNotifyGive(hXiaomiBLE);

  // Wait for first valid Xiaomi reading, max 2 minutes
  TickType_t xWaitStart = xTaskGetTickCount();
  while (!g_xiaomiValid && (xTaskGetTickCount() - xWaitStart < pdMS_TO_TICKS(120000))) //  minutes timeout to get the initial value on the xiaomi sensor
      vTaskDelay(pdMS_TO_TICKS(500));

  if (hTaskCloud)       xTaskNotifyGive(hTaskCloud);
  if (hHomeTempControl) xTaskNotifyGive(hHomeTempControl);


  for (;;) {
    if (hHeater)        xTaskNotifyGive(hHeater);
    vTaskDelayUntil(&xLastWakeTime, xInterval);

    if (hWaterActuator) xTaskNotifyGive(hWaterActuator);
    vTaskDelayUntil(&xLastWakeTime, xInterval);

    if (hNTC)           xTaskNotifyGive(hNTC);
    vTaskDelayUntil(&xLastWakeTime, xInterval);

    // --- CONDITIONALLY NOTIFY CLOUD (every 30 s) ---
    if (hTaskCloud && (xTaskGetTickCount() - xLastCloudTime >= xCloudInterval)) {
        xTaskNotifyGive(hTaskCloud);
        xLastCloudTime = xTaskGetTickCount();
    }
    vTaskDelayUntil(&xLastWakeTime, xInterval);

    // --- CONDITIONALLY NOTIFY XIAOMI + PI (every 5 min) ---
    if (hXiaomiBLE && (xTaskGetTickCount() - xLastXiaomiTime >= xXiaomiInterval)) {
        xTaskNotifyGive(hXiaomiBLE);
        if (hHomeTempControl) xTaskNotifyGive(hHomeTempControl);
        xLastXiaomiTime = xTaskGetTickCount();
    }

    if (hHeapMonitor)   xTaskNotifyGive(hHeapMonitor);
    vTaskDelayUntil(&xLastWakeTime, xInterval);

    if (hWatchdog)      xTaskNotifyGive(hWatchdog);
    vTaskDelayUntil(&xLastWakeTime, xInterval);

 

  }
}