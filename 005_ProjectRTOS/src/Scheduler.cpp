
#include "ProjectHeater.h" // For D_PRINTLN

void TaskTimeScheduler(void* pv) {
  (void)pv;
  TickType_t xLastWakeTime = xTaskGetTickCount();
  const TickType_t xInterval = pdMS_TO_TICKS(250); 
  
  // Track when we last sent data to the cloud
  TickType_t xLastCloudTime = xTaskGetTickCount();
  const TickType_t xCloudInterval = pdMS_TO_TICKS(15000); // 15 seconds push data to the hive

  for (;;) {
    if (hStepper)     xTaskNotifyGive(hStepper);
    vTaskDelayUntil(&xLastWakeTime, xInterval);

    if (hDC)          xTaskNotifyGive(hDC);
    vTaskDelayUntil(&xLastWakeTime, xInterval);

    if (hServo)       xTaskNotifyGive(hServo);
    vTaskDelayUntil(&xLastWakeTime, xInterval);

    if (hTherm)       xTaskNotifyGive(hTherm);
    vTaskDelayUntil(&xLastWakeTime, xInterval);

    if (hDHT)         xTaskNotifyGive(hDHT);
    vTaskDelayUntil(&xLastWakeTime, xInterval);

    if (hMic)         xTaskNotifyGive(hMic);
    vTaskDelayUntil(&xLastWakeTime, xInterval);

    // --- CONDITIONALLY NOTIFY CLOUD ---
    // Check if 60 seconds have passed since the last cloud update
    if (hTaskCloud && (xTaskGetTickCount() - xLastCloudTime >= xCloudInterval)) {
        xTaskNotifyGive(hTaskCloud); 
        xLastCloudTime = xTaskGetTickCount(); // Reset the timer
        D_PRINTLN(F("☁️ Cloud Update Triggered"));
    }
    // We still delay here to keep the 250ms "rhythm" for the other tasks
    vTaskDelayUntil(&xLastWakeTime, xInterval);

    if (hHeapMonitor) xTaskNotifyGive(hHeapMonitor);
    vTaskDelayUntil(&xLastWakeTime, xInterval);

    if (hWatchdog)    xTaskNotifyGive(hWatchdog);
    vTaskDelayUntil(&xLastWakeTime, xInterval);

    D_PRINTLN(F("--- Cycle Complete ---"));
  }
}