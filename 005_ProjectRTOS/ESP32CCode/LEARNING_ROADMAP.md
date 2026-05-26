# Heater Project — Learning Roadmap

**Author:** Dalia Oueidat
**Started:** 2026-05-25

---

## Step 1 — Migrate to ESP32-S3
*Goal: same application, new hardware*

1. Fix `platformio.ini` — replace incompatible libraries
2. Fix `ProjectHeater.h` — replace UNO R4 headers (`WiFiS3`, `WDT`, `RTC`, `Arduino_FreeRTOS`)
3. Fix `DataToCloud.cpp` — replace `NVIC_SystemReset()`, `WDT.refresh()`, NTP sync, `WiFiSSLClient`
4. Fix `main.cpp` — replace WDT API
5. Fix stack sizes — ESP32 needs larger stacks
6. Confirm full system running on ESP32-S3

**What you gain:** familiarity with ESP32, understand what is hardware-specific vs portable

---

## Step 2 — Learn Dual-Core Issues
*Goal: understand real parallelism bugs*
*No hardware needed — all exercises use global variables and Serial output only*

### Exercise 1 — Race Condition
- Task A (Core 0) increments a shared counter 1,000,000 times
- Task B (Core 1) decrements the same counter 1,000,000 times
- Expected result: counter = 0
- Actual result: random number — both cores stomped on each other
- Fix: protect the counter with `portENTER_CRITICAL()` / mutex
- Observe: counter returns to 0 every time after the fix

### Exercise 2 — Data Corruption
- Task A (Core 0) writes a float (e.g. temperature) in a loop
- Task B (Core 1) reads it and prints it
- Observe: garbage values when read mid-write
- Fix: use a mutex around read and write
- Learn: why `volatile` alone is not enough on multi-core

### Exercise 3 — Deadlock
- Task A holds Mutex1, then tries to take Mutex2
- Task B holds Mutex2, then tries to take Mutex1
- Observe: both tasks freeze forever, Serial goes silent
- Fix: always take mutexes in the same order across all tasks

### Exercise 4 — Priority Inversion
- Task Low (priority 1) takes a mutex and does slow work
- Task High (priority 3) tries to take the same mutex — gets blocked by Task Low
- Task Mid (priority 2) runs freely and starves Task High
- Observe: high-priority task blocked by low-priority task
- Fix: enable priority inheritance on the mutex

### Exercise 5 — Core Pinning
- Create tasks without pinning — let FreeRTOS assign cores freely
- Observe which core each task lands on via `xPortGetCoreID()`
- Pin specific tasks to Core 0 or Core 1 with `xTaskCreatePinnedToCore()`
- Learn: when to pin (time-critical tasks) vs when to let the scheduler decide

### Exercise 6 — Cache Coherency
- Task A (Core 0) writes a variable in a tight loop
- Task B (Core 1) reads it and checks if it sees the update immediately
- Observe: stale values due to each core's cache
- Fix: use `volatile` + memory barriers, or pass data via FreeRTOS queue
- Learn: why queues are the safest way to share data between cores

**What you gain:** real production-level RTOS knowledge, debugging skills, confidence with concurrent systems

---

## Step 3 — Switch to Zephyr RTOS
*Goal: portability and professional RTOS*

1. **Setup** — install Zephyr SDK, learn CMake + west build system
2. **Devicetree** — describe your ESP32-S3 hardware in `.dts` (pins, peripherals)
3. **Kconfig** — configure the kernel (stack sizes, features)
4. **Threads** — rewrite your FreeRTOS tasks as Zephyr threads
5. **Synchronization** — rewrite mutexes, semaphores using Zephyr API
6. **WiFi + MQTT** — rewire networking layer without Arduino libraries
7. **Portability test** — run the same code on a second board with minimal changes

**What you gain:** industry-level skills, true hardware portability, Zephyr is used in automotive and industrial IoT

---

## The Big Picture

```
UNO R4 + FreeRTOS          done
ESP32-S3 + FreeRTOS        Step 1 (migration)
ESP32-S3 + FreeRTOS        Step 2 (dual-core mastery)
ESP32-S3 + Zephyr          Step 3 (portability)
Any board + Zephyr         graduation
```

Each step builds directly on the previous one — nothing is thrown away, everything compounds.
