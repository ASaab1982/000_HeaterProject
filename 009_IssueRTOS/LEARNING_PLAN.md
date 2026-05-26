# RTOS Issues — Learning Plan

**Platform:** ESP32-S3 + FreeRTOS (via Arduino framework)
**Folder:** `009_IssueRTOS/`
**No hardware needed** — all exercises use Serial output only

---

## Progress

| # | Exercise | Concept | Status |
|---|---|---|---|
| 1 | Race Condition | Two cores destroy each other's writes | ✅ Done |
| 2 | Data Corruption | Shared float read mid-write → garbage | ⬜ Next |
| 3 | Deadlock | Two tasks wait on each other forever | ⬜ |
| 4 | Priority Inversion | Low priority task starves high priority task | ⬜ |
| 5 | Core Pinning | Control which core runs which task | ⬜ |
| 6 | Cache Coherency | Stale values across cores — why queues are safer | ⬜ |

---

## Exercise 1 — Race Condition ✅
**File:** `EXERCISE_01_RACE_CONDITION.md`

**The bug:** two cores increment/decrement a shared counter simultaneously.
`g_counter++` is 3 instructions (read, add, write) — one core can overwrite the other mid-operation.

**The fix:** `portENTER_CRITICAL` spinlock — only one core inside the critical section at a time.

**Key lesson:** RTOS does not prevent race conditions. It gives you the tools. You must use them.

---

## Exercise 2 — Data Corruption
**The bug:** Task A writes a float (e.g. temperature) in a loop.
Task B reads it and prints it.
A float is 4 bytes — the CPU can be interrupted between writing byte 2 and byte 3.
Task B reads a half-old, half-new value → garbage number on Serial.

**The fix:** mutex (`xSemaphoreCreateMutex`) — Task B sleeps while Task A is writing,
instead of spinning like a spinlock. Correct because the operation is longer than a few instructions.

**Key lesson:** `volatile` alone is not enough on multi-core. It prevents compiler caching
but does not prevent the CPU from being interrupted mid-write.

---

## Exercise 3 — Deadlock
**The bug:**
- Task A (Core 0) takes Mutex1, then tries to take Mutex2
- Task B (Core 1) takes Mutex2, then tries to take Mutex1
- Both tasks wait for each other forever → Serial goes silent → system appears frozen

**The fix:** always take mutexes in the same order across all tasks.
If every task takes Mutex1 before Mutex2, deadlock is impossible.

**Key lesson:** deadlock requires four conditions — mutual exclusion, hold and wait,
no preemption, circular wait. Break any one and deadlock cannot happen.

---

## Exercise 4 — Priority Inversion
**The bug:**
- Task Low  (priority 1) takes a mutex and does slow work
- Task High (priority 3) tries to take the same mutex → blocked by Task Low
- Task Mid  (priority 2) runs freely and starves Task High
- A high priority task is blocked by a low priority task — the priority system is inverted

**The fix:** priority inheritance — when Task High blocks on a mutex held by Task Low,
FreeRTOS temporarily raises Task Low to priority 3 so Task Mid cannot starve it.
Task Low finishes fast, releases the mutex, Task High runs.

**Key lesson:** priority inheritance is not automatic in FreeRTOS — you must create
the mutex with `xSemaphoreCreateMutex()`, not a binary semaphore, to get it.

---

## Exercise 5 — Core Pinning
**The concept:** on ESP32 dual-core, FreeRTOS assigns tasks to cores automatically.
You can override this with `xTaskCreatePinnedToCore()`.

**What we explore:**
- Create tasks without pinning → observe which core they land on via `xPortGetCoreID()`
- Pin time-critical tasks to Core 1 (Core 0 is shared with WiFi stack)
- Pin a task to Core 0 and observe WiFi interference on timing
- Learn when to pin vs when to let the scheduler decide

**Key lesson:** Core 0 runs the WiFi/BT stack. Pinning a real-time task to Core 0
risks timing jitter from WiFi interrupts. Pin sensitive tasks to Core 1.

---

## Exercise 6 — Cache Coherency
**The bug:** each core has its own cache. Core 0 writes a variable — Core 1 may
read a stale cached copy and not see the update immediately.
`volatile` forces the compiler to re-read from RAM but does not flush the CPU cache.

**The fix:** pass data between cores via a FreeRTOS queue (`xQueueCreate`).
The queue API includes the correct memory barriers — data is always fresh on the receiving core.

**Key lesson:** queues are the safest way to share data between cores.
They solve cache coherency, race conditions, and data corruption in one pattern.

---

## The Big Picture

```
Exercise 1 — Race Condition       → portENTER_CRITICAL (spinlock)
Exercise 2 — Data Corruption      → xSemaphoreCreateMutex (mutex)
Exercise 3 — Deadlock             → consistent lock ordering
Exercise 4 — Priority Inversion   → priority inheritance mutex
Exercise 5 — Core Pinning         → xTaskCreatePinnedToCore
Exercise 6 — Cache Coherency      → xQueueCreate (queues)
```

Each exercise introduces one new RTOS concept and one new API.
By Exercise 6 you have the full toolkit for safe concurrent embedded systems.
