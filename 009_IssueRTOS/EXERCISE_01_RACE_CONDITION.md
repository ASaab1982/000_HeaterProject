# Exercise 1 — Race Condition

## What we built
Two FreeRTOS tasks pinned to separate cores share one counter.
- Task A (Core 0): increments 1,000,000 times
- Task B (Core 1): decrements 1,000,000 times
- Expected result: 0
- Actual result without protection: never 0

---

## The bug — why it happens

`g_counter++` is NOT one instruction. The CPU breaks it into three:

```
1. READ  g_counter from RAM into register
2. ADD   1 to the register
3. WRITE register back to RAM
```

Both cores do this at the same time with no coordination:

```
Core 0: READ (100) → ADD → WRITE 101
Core 1: READ (100)              → SUB → WRITE 99   ← overwrites 101, increment lost
```

One write destroyed the other. This is a race condition.

---

## Key concepts learned

### Sequential vs RTOS tasks
| Sequential functions | RTOS tasks |
|---|---|
| You control the order | Scheduler controls the order |
| One thing at a time | Two cores can run simultaneously |
| No race conditions possible | Race conditions possible |
| Simple scripts | Concurrent real-time systems |

### FreeRTOS on ESP32
- FreeRTOS is included automatically via `Arduino.h` → Arduino framework → ESP-IDF
- You never install or start it manually
- It is always running before `setup()` is called
- Even `loop()` runs inside a FreeRTOS task called `loopTask`

### The FreeRTOS kernel scheduler
- Always running in the background
- Decides which task runs on which core
- Fires every 1ms (1000 Hz tick) — that is 240,000 CPU cycles per tick at 240MHz
- You talk to it via API: `xTaskCreate`, `vTaskDelay`, `ulTaskNotifyTake`, priorities

### RTOS does NOT prevent race conditions
RTOS gives you the **tools** to prevent them. You must use those tools.
A race condition can happen with or without RTOS if shared variables are unprotected.

---

## The fix — spinlock

```cpp
static portMUX_TYPE g_mux = portMUX_INITIALIZER_UNLOCKED;

portENTER_CRITICAL(&g_mux);
g_counter++;                  // now atomic — no other core can enter here
portEXIT_CRITICAL(&g_mux);
```

### How the spinlock works
1. Core 0 enters critical section — locks `g_mux`
2. Core 1 tries to enter — `g_mux` is taken → Core 1 **spins** (busy waits)
3. Core 0 finishes and releases `g_mux`
4. Core 1 locks `g_mux` and proceeds
5. The full read-modify-write is now one unbreakable unit

### What portENTER_CRITICAL also does
While the spinlock is held, on that core:
- Interrupts are DISABLED
- FreeRTOS tick is DISABLED (scheduler cannot switch tasks)
- WiFi stack cannot service its interrupts
- WDT cannot be kicked

---

## Spinlock vs Mutex

| | Spinlock `portENTER_CRITICAL` | Mutex `xSemaphoreCreateMutex` |
|---|---|---|
| Blocked core | Burns CPU spinning | Sleeps — scheduler runs other tasks |
| When to use | Very short operations (< ~1µs) | Longer operations |
| Overhead | Zero | Small (scheduler sleep/wake) |
| Can use inside ISR | Yes | No |

**Rule:** if the protected operation is a few instructions → spinlock.
If it takes longer → mutex (Exercise 2 uses a mutex).

### Spinlock rules
- Keep the critical section as short as possible
- Never call `vTaskDelay` inside — scheduler is frozen → deadlock
- Never call `Serial.print` inside — UART interrupt disabled → corrupted output
- Never hold it longer than ~1µs — WiFi/BT miss their timing windows
- You can have as many **separate** spinlocks as you want — one per shared variable

---

## taskYIELD()
Tells the scheduler: give up my CPU time for one tick (~1ms), then I am ready again.
- Shortest possible pause
- Does not sleep — just lets something else go first this tick
- Used in the exercise to try to break timing determinism

---

## Result
| Mode | Result every round |
|---|---|
| BUG (`FIX_RACE 0`) | Never 0 — writes are lost |
| FIX (`FIX_RACE 1`) | Always 0 — every write protected |
