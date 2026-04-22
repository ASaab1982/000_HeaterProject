
#include "ProjectHeater.h" // For D_PRINTLN

void TaskTimeScheduler(void* pv) {
  (void)pv;
  TickType_t xLastWakeTime = xTaskGetTickCount();
  const TickType_t xInterval = pdMS_TO_TICKS(250); 

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

    if (hWebPost)     xTaskNotifyGive(hWebPost); 
    vTaskDelayUntil(&xLastWakeTime, xInterval);

    if (hHeapMonitor) xTaskNotifyGive(hHeapMonitor);
    vTaskDelayUntil(&xLastWakeTime, xInterval);

    if (hWatchdog)    xTaskNotifyGive(hWatchdog);
    vTaskDelayUntil(&xLastWakeTime, xInterval);

    D_PRINTLN(F("--- Cycle Complete ---"));
  }
}













