// ============================================================
// Exercise 1 — Race Condition
//
// Two tasks run on separate cores and share one counter.
// Task A increments it 1,000,000 times.
// Task B decrements it 1,000,000 times.
// Expected result: counter == 0
// Actual result:   different wrong number every run
//
// To see the bug:  set FIX_RACE 0, flash, open Serial Monitor
// To see the fix:  set FIX_RACE 1, flash, open Serial Monitor
// ============================================================

#define FIX_RACE 1   // 0 = bug | 1 = fix

#include <Arduino.h>

static volatile int32_t g_counter = 0;
static portMUX_TYPE     g_mux     = portMUX_INITIALIZER_UNLOCKED;

static volatile bool taskA_done = false;
static volatile bool taskB_done = false;
static int           roundNum   = 0;

// --------------------------------------------------------
// Task A — increments 1,000,000 times (Core 0)
// --------------------------------------------------------
void taskA(void*)
{
    for (int32_t i = 0; i < 1000000; i++)
    {
#if FIX_RACE
        portENTER_CRITICAL(&g_mux);
        g_counter++;
        portEXIT_CRITICAL(&g_mux);
#else
        g_counter++;        // unsafe — race here
#endif
    }
    taskA_done = true;
    vTaskDelete(nullptr);
}

// --------------------------------------------------------
// Task B — decrements 1,000,000 times (Core 1)
// --------------------------------------------------------
void taskB(void*)
{
    for (int32_t i = 0; i < 1000000; i++)
    {
#if FIX_RACE
        portENTER_CRITICAL(&g_mux);
        g_counter--;
        portEXIT_CRITICAL(&g_mux);
#else
        g_counter--;        // unsafe — race here
#endif
    }
    taskB_done = true;
    vTaskDelete(nullptr);
}

static void startRound()
{
    g_counter  = 0;
    taskA_done = false;
    taskB_done = false;
    xTaskCreatePinnedToCore(taskA, "TaskA", 4096, nullptr, 1, nullptr, 0);
    xTaskCreatePinnedToCore(taskB, "TaskB", 4096, nullptr, 1, nullptr, 1);
}

void setup()
{
    Serial.begin(115200);
    delay(1000);
    Serial.println("========================================");
    Serial.println(" Exercise 1 — Race Condition");
    Serial.println(" Task A: +1,000,000 | Task B: -1,000,000");
    Serial.println(" Expected result always: 0");
#if FIX_RACE
    Serial.println(" Mode: FIX (portENTER_CRITICAL)");
#else
    Serial.println(" Mode: BUG (no protection)");
#endif
    Serial.println("========================================");
    startRound();
}

void loop()
{
    if (!taskA_done || !taskB_done)
        return;

    roundNum++;
    int32_t result   = g_counter;
    int32_t lost_ops = abs(result);
    float   lost_pct = (lost_ops / 1000000.0f) * 100.0f;

    Serial.print("Round ");
    Serial.print(roundNum);
    Serial.print("  result: ");
    Serial.print(result);
    Serial.print("   lost writes: ");
    Serial.print(lost_ops);
    Serial.print(" (");
    Serial.print(lost_pct, 1);
    Serial.println("%)");

    delay(300);
    startRound();
}
