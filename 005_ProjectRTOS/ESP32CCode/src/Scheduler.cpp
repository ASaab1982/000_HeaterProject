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
 *   Slot 4 — TaskDHT
 *   Slot 5 — TaskCloud (only every 5 min, not every cycle)
 *   Slot 6 — TaskMonitor (heap stats)
 *   Slot 7 — TaskwatchdogMonitor
 *   + TaskXiaomiBLE notified once, exactly 2 min after each cloud push
 *
 * One full round takes 7 × 250 ms = 1.75 seconds.
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
  const TickType_t xCloudInterval  = pdMS_TO_TICKS(300000);
  const TickType_t xXiaomiOffset   = pdMS_TO_TICKS(120000);  // 2 min after cloud push

  TickType_t xCloudFireTick  = 0;      // tick when cloud last fired
  bool       xiaomiPending   = false;  // true once cloud fires, cleared when Xiaomi is notified

  for (;;) {
    if (hHeater)        xTaskNotifyGive(hHeater);
    vTaskDelayUntil(&xLastWakeTime, xInterval);

    if (hWaterActuator) xTaskNotifyGive(hWaterActuator);
    vTaskDelayUntil(&xLastWakeTime, xInterval);

    if (hHeaterTemp)    xTaskNotifyGive(hHeaterTemp);
    vTaskDelayUntil(&xLastWakeTime, xInterval);


    if (hDHT)           xTaskNotifyGive(hDHT);
    vTaskDelayUntil(&xLastWakeTime, xInterval);

    // --- CONDITIONALLY NOTIFY CLOUD (every 15 s) ---
    if (hTaskCloud && (xTaskGetTickCount() - xLastCloudTime >= xCloudInterval)) {
        xTaskNotifyGive(hTaskCloud);
        xLastCloudTime = xTaskGetTickCount();
        xCloudFireTick = xLastCloudTime;  // arm the Xiaomi 7-second delay
        xiaomiPending  = true;
    }
    vTaskDelayUntil(&xLastWakeTime, xInterval);

    if (xiaomiPending && hXiaomiBLE &&
        (xTaskGetTickCount() - xCloudFireTick >= xXiaomiOffset)) {
        xTaskNotifyGive(hXiaomiBLE);
        xiaomiPending = false;
    }

    if (hHeapMonitor)   xTaskNotifyGive(hHeapMonitor);
    vTaskDelayUntil(&xLastWakeTime, xInterval);

    if (hWatchdog)      xTaskNotifyGive(hWatchdog);
    vTaskDelayUntil(&xLastWakeTime, xInterval);

    // --- CONDITIONALLY NOTIFY XIAOMI BLE (7 s after cloud, once per cloud cycle) ---

  }
}