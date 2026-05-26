# Heater Project — Work Document

**Author:** Dalia Oueidat  
**Platform:** Arduino UNO R4 WiFi + FreeRTOS + Node.js + HiveMQ  
**Repository branch:** `DevelopmentASaab` → merged to `main` for production  
**Production URL:** https://zero00-heaterproject.onrender.com

---

## 1. Project Goal

Build a complete IoT smart heating system that:
- Simulates a real boiler and home thermal environment
- Controls the heater relay, water pump, and servo valve automatically
- Allows remote monitoring and control from a web UI (desktop + mobile)
- Is robust enough to run unattended 24/7 with automatic recovery from failures

---

## 2. System Architecture

```
Arduino UNO R4 WiFi (FreeRTOS)
        |
        |  MQTT over TLS — port 8883
        |  publishes every 15s  → boilers/B1/status  (JSON, retained)
        |  subscribes          ← boilers/B1/commands (JSON)
        |  subscribes          ← boilers/B1/weather  (retained, from Node.js)
        v
HiveMQ Cloud Broker (Free Serverless — AWS EU)
  d72dc8b632b04c8c91c4702a5b164d59.s1.eu.hivemq.cloud
        |
        |  MQTT over TLS — port 8883
        v
Node.js Bridge Server (Render.com free tier)
  Express + Socket.io + mqtt.js
  Fetches real outdoor weather from Open-Meteo every 10 min
        |
        |  WebSocket (Socket.io)
        v
Browser / Mobile  (index.html — dark UI, responsive)
```

---

## 3. Hardware

| Component | Pin | Role |
|---|---|---|
| Arduino UNO R4 WiFi | — | Renesas RA4M1, Cortex-M4F with FPU, FreeRTOS |
| DHT11 sensor | D2 | Ambient temperature and humidity (inside enclosure) |
| Heater relay | D11 | Switches physical heating element ON/OFF |
| H-bridge motor driver | IN1=D8, IN2=D7, EN=D6 | Drives water circulation pump |
| Servo motor | D5 | Controls water valve position (0°–180°) |
| Analog temp sensor | A0 | (available, not currently used in active tasks) |
| Analog water sensor | A1 | (available, not currently used in active tasks) |

---

## 4. Firmware Architecture (FreeRTOS)

### 4.1 Task Overview

All tasks except `TaskTimeScheduler` and `TaskCloud` block on `ulTaskNotifyTake()` — they do zero work until the scheduler wakes them. This avoids busy-waiting and keeps CPU usage minimal.

| Task | Handle | Priority | Stack | File | Role |
|---|---|---|---|---|---|
| TaskTimeScheduler | hTimeScheduler | **3** | 50 words | Scheduler.cpp | Master heartbeat — notifies all tasks in sequence |
| TaskCloud | hTaskCloud | **2** | 400 words | DataToCloud.cpp | WiFi/MQTT, telemetry, command handling |
| TaskHeater | hHeater | 1 | 100 words | Heater.cpp | Heater relay + automatic thermostat |
| TaskWaterActuator | hWaterActuator | 1 | 100 words | WaterActuator.cpp | Pump + valve proportional control |
| TaskHeaterTemp | hHeaterTemp | 1 | 150 words | Sensors.cpp | Boiler water temperature simulation |
| TaskHomeTemp | hHomeTemp | 1 | 150 words | Sensors.cpp | Home air temperature simulation |
| TaskDHT | hDHT | 1 | 130 words | Sensors.cpp | DHT11 physical sensor read |
| TaskMonitor | hHeapMonitor | 1 | 60 words | HeapMonitor.cpp | Stack HWM + heap diagnostics |
| TaskwatchdogMonitor | hWatchdog | 1 | 60 words | main.cpp | Hardware WDT kick |

**Why TaskCloud is priority 2:**  
The Arduino WiFi library's `mqttClient.connect()` performs a TLS handshake that is entirely blocking — it holds the CPU for several seconds with no internal yield. At priority 1 (equal to all other tasks), the 250ms SysTick causes context switches mid-handshake, each pushing 18 FPU words onto the task stack (ARM Cortex-M4F lazy FPU save), eventually overflowing the 400-word stack. At priority 2, no lower-priority task can preempt it during the handshake — the stack never overflows.

**Why TaskCloud stack is 400 words:**  
TLS handshake + MQTT + ArduinoJson + String serialization = deep call stack. Allocated first in `setup()` before any other task, while heap is still contiguous, to guarantee a 400-word contiguous block.

### 4.2 Scheduler Timing

TaskTimeScheduler runs at priority 3 and uses `vTaskDelayUntil()` for a precise 250ms rhythm:

```
Slot 1 (+0ms)    → notify TaskHeater
Slot 2 (+250ms)  → notify TaskWaterActuator
Slot 3 (+500ms)  → notify TaskHeaterTemp
Slot 4 (+750ms)  → notify TaskHomeTemp
Slot 5 (+1000ms) → notify TaskDHT
Slot 6 (+1250ms) → notify TaskCloud (only every 15s)
Slot 7 (+1500ms) → notify TaskMonitor
Slot 8 (+1750ms) → notify TaskwatchdogMonitor
```

One full round = 8 × 250ms = 2 seconds.  
Cloud telemetry fires every 15 seconds (~7–8 rounds).

### 4.3 System Health Watchdog

Each task sets a bit in `systemHealth` after completing its work each cycle:

| Bit | Task | Mask |
|---|---|---|
| 0 | WaterActuator (pump) | `1 << 0` |
| 1 | HomeTemp | `1 << 1` |
| 2 | HeaterTemp | `1 << 2` |
| 3 | DHT | `1 << 3` |
| 4 | Heater | `1 << 4` |
| 5 | WaterActuator (valve) | `1 << 5` |

`HEALTH_REQUIRED = 0x3F` (all 6 bits set).

`TaskwatchdogMonitor` checks `systemHealth == HEALTH_REQUIRED` and only then calls `WDT.refresh()`. If any task failed to run, the bit is missing, the hardware WDT fires at ~5.5s, and the system resets.

`systemHealth` is cleared to `0x00` at the end of every cycle regardless of the outcome.

**WDT during initialisation:**  
`initializeCloud()` can take 15+ seconds (WiFi + NTP + TLS handshake with up to 10 retries). During this time worker tasks at priority 1 cannot run (TaskCloud at priority 2 holds the CPU during blocking calls), so health bits are never set and the watchdog would not kick the WDT. To prevent a reset during legitimate initialisation, `WDT.refresh()` is called directly at three points inside `initializeCloud()`:
1. After WiFi connects (before NTP + MQTT setup)
2. Before each `mqttClient.connect()` TLS attempt
3. Immediately after a successful MQTT connection

---

## 5. Physics Models

### 5.1 HeaterModel — Boiler Water Temperature

**File:** `src/HeaterModel.cpp`  
**Called by:** `doHeaterTempRead()` every ~2s  
**Variable updated:** `g_heaterWaterTemp`

**Heater ON:**
```
g_heaterWaterTemp += (1/20) × Δt    [1°C per 20 seconds]
cap at 90°C
```

**Heater OFF (cooling, outdoor-dependent):**
```
coolingRate = interpolate(g_outdoorTemp, −5°C → 1/600, +30°C → 1/1200)
g_heaterWaterTemp -= coolingRate × Δt
floor at g_outdoorTemp
```

Cooling rate is linearly interpolated between the two endpoints and clamped — no extrapolation outside [−5°C, +30°C].

*This model will be replaced by a real boiler water temperature sensor in a future release.*

### 5.2 HomeTempModel — Home Air Temperature

**File:** `src/HomeTempModel.cpp`  
**Called by:** `doHomeTempRead()` every ~2s  
**Variable updated:** `g_homeTemp`  
**Constants (shared with WaterActuator):** defined in `src/HomeTempModel.h`

```
HEAT_GAIN_RATE = 1 / 900    [°C/s per °C difference, boiler → home]
HEAT_LOSS_RATE = 1 / 86400  [°C/s per °C difference, home ↔ outdoor]
```

**Each time step:**
```
gainFactor = 0.10 + (g_waterValvePosition / 180) × 0.90   [10% at 0°, 100% at 180°]

IF pump is running (g_WaterPumpSpeed > 0):
    gain = HEAT_GAIN_RATE × gainFactor × (g_heaterWaterTemp − g_homeTemp) × Δt
    g_homeTemp += gain   [only if gain > 0]

exchange = HEAT_LOSS_RATE × (g_homeTemp − g_outdoorTemp) × Δt
g_homeTemp -= exchange   [bidirectional: negative exchange = home gains heat from warm outdoor]
```

**Key behaviour:**
- Home equilibrates with outdoors in ~24 hours (no heating, no loss compensation)
- At full valve open (180°) and 40°C boiler → home reaches boiler temp in ~15 minutes
- Valve at 0° still allows 10% heat transfer (minimum flow)

---

## 6. Control Strategy — WaterActuator

**File:** `src/WaterActuator.cpp`  
**Variables controlled:** `g_WaterPumpSpeed`, `g_waterValvePosition`

### 6.1 3-Zone Logic with Hysteresis

```
deadband   = 0.5°C
hysteresis = 0.1°C  (applied to each boundary side)

Zone 1 — Too cold:    g_homeTemp < targetHomeTemp − 0.6°C
    → pump ON (full speed), valve 180° (full heat)

Zone 2 — Equilibrium: targetHomeTemp − 0.4°C ≤ g_homeTemp ≤ targetHomeTemp + 0.4°C
    → pump ON, valve at analytically computed equilibrium position

Zone 3 — Too warm:    g_homeTemp > targetHomeTemp + 0.6°C
    → pump OFF, valve 0° (no heat)
```

**Hysteresis prevents rapid zone switching.** Zone transitions require passing 0.2°C past the boundary before switching.

### 6.2 Equilibrium Valve Position (Zone 2)

In Zone 2, the valve angle is computed so that heat gain from the boiler exactly equals heat loss to outdoors:

```
Solve: HEAT_GAIN_RATE × gainFactor × (g_heaterWaterTemp − g_homeTemp) = HEAT_LOSS_RATE × (g_homeTemp − g_outdoorTemp)

gainFactor = (HEAT_LOSS_RATE × (g_homeTemp − g_outdoorTemp)) / (HEAT_GAIN_RATE × (g_heaterWaterTemp − g_homeTemp))

valvePos = ((gainFactor − 0.10) / 0.90) × 180     [maps gainFactor back to servo degrees]
valvePos = constrain(valvePos, 0, 180)
```

If `g_heaterWaterTemp ≤ g_homeTemp`, valve stays at 0° (boiler too cold to heat home).

---

## 7. Heater Thermostat Logic

**File:** `src/Heater.cpp`  
**Variable controlled:** `heaterState` (bool)

### 7.1 Manual Override ON

`heaterState` is set directly from the UI via MQTT command (`{"command":"heater","action":"on"|"off"}`). The relay pin follows `heaterState` each cycle with no further logic.

### 7.2 Manual Override OFF (Automatic Mode)

5°C hysteresis prevents rapid relay cycling:
```
IF g_heaterWaterTemp < heaterTempSetPoint       → heaterState = true  (turn ON)
IF g_heaterWaterTemp > heaterTempSetPoint + 5°C → heaterState = false (turn OFF)
[between setpoint and setpoint+5°C: no change]
```

`heaterTempSetPoint` is set from the UI (40–70°C range, enforced on Arduino).

---

## 8. MQTT Communication

### 8.1 Topics

| Topic | Direction | Format | Retained |
|---|---|---|---|
| `boilers/B1/status` | Arduino → Cloud | JSON | Yes |
| `boilers/B1/commands` | Cloud → Arduino | JSON | No |
| `boilers/B1/weather` | Node.js → Arduino | JSON | Yes |

### 8.2 Status JSON (published every 15s)

```json
{
  "deviceId": "B1",
  "waterpumpactivation": 100,
  "heaterWaterTemp": 55.3,
  "homeTempC": 21.4,
  "dhtTempC": 8.2,
  "dhtHumidity": 73.1,
  "heaterActivation": "ON",
  "waterValvePosition": 90,
  "systemHealth": 63,
  "heaterState": true,
  "targetHomeTemp": 21.0,
  "heaterTempSetPoint": 60.0,
  "manualOverride": false
}
```

### 8.3 Commands Received

```json
{ "command": "heater",         "action": "on"|"off" }         // manual override only
{ "command": "temperature",    "value": 21.5 }                 // targetHomeTemp (5–30°C)
{ "command": "manualOverride", "value": 0|1 }                  // toggle auto/manual
{ "command": "heaterSetPoint", "value": 60.0 }                 // boiler setpoint (40–70°C)
{ "command": "weather",        "outdoorTemp": 8.0, "outdoorHumidity": 73 }
```

Safety clamps are enforced on the Arduino regardless of what the server sends.

### 8.4 Weather Integration

Node.js fetches real outdoor temperature and humidity from Open-Meteo (Neirivue coordinates) on startup and every 10 minutes, then publishes it to `boilers/B1/weather` as a retained MQTT message. The Arduino subscribes to this topic on connect and immediately receives the latest outdoor data.

When `g_useCloudWeather = true`, `doDHTRead()` skips the physical DHT11 read so real outdoor values from Open-Meteo are not overwritten by indoor sensor noise.

### 8.5 MQTT Client IDs

| Context | clientId |
|---|---|
| Arduino firmware | `Arduino_Heater_Unit_B1` |
| Node.js — local dev | `Server_Bridge_Local` |
| Node.js — production (Render) | `Server_Bridge_Main` |

Fixed IDs prevent HiveMQ from creating a new session on each reconnect (session stacking).

---

## 9. Resilience and Recovery

| Mechanism | Trigger | Action |
|---|---|---|
| Hardware WDT (~5.5s) | Any task stops reporting | System reset via WDT fire |
| WDT init kicks | `initializeCloud()` blocking calls | `WDT.refresh()` called at 3 explicit points |
| Dual WiFi fallback | SSID1 fails (5s timeout) | Try SSID2 |
| MQTT Last Will & Testament | Arduino drops off HiveMQ | Broker publishes "offline" to status topic |
| MQTT online message | Successful reconnect | Arduino publishes "online" to status topic |
| 10-minute offline reset | MQTT disconnected > 10 min | `NVIC_SystemReset()` for full recovery |
| Stale data watchdog (Node.js) | No MQTT message for 90s | UI shows "NO DATA" orange banner |

---

## 10. Diagnostic Output (Serial Monitor)

Only the Cycle Stats block is printed — all other runtime prints are removed.  
All output is wrapped in `D_PRINT` / `D_PRINTLN` macros (active only when `DEBUG = 1`).

```
====== Cycle Stats ======
Heater    : <HWM words>
WaterAct  : <HWM words>
HomeTemp  : <HWM words>
HeaterTemp: <HWM words>
DHT       : <HWM words>
HeapMon   : <HWM words>
Scheduler : <HWM words>
WatchDog  : <HWM words>
CloudPost : <HWM words>
FreeHeap  : <bytes>
MinEver   : <bytes>
Health    : 111111
=========================
```

HWM = high-water mark = minimum free stack words remaining. A value approaching 0 means stack overflow risk.

---

## 11. Node.js Bridge Server

**Location:** `BoilerServerOnRTOS/`  
**Stack:** Node.js + Express + Socket.io + mqtt.js  
**Deployed:** Render.com (free tier, auto-deploys from `main` branch)

**Key responsibilities:**
- Bridge between HiveMQ (MQTT) and browsers (WebSocket)
- Weather fetch from Open-Meteo and injection via MQTT
- Login gate: credentials from environment variables, authorized users join a Socket.io room
- Stale-data watchdog: checks every 30s, marks boiler as no-data after 90s silence

**Environment variables (stored on Render, never in git):**
```
MQTT_USERNAME    HiveMQ username
MQTT_PASSWORD    HiveMQ password
WEB_USERNAME     Web UI login username
WEB_PASSWORD     Web UI login password
PORT             Automatically set by Render
```

---

## 12. Web UI

**Location:** `BoilerServerOnRTOS/public/index.html`  
**Stack:** HTML + CSS + Socket.io client (no framework)

**Layout:** Two-column desktop, single-column mobile (breakpoint at 700px)

**Data displayed:**
- Sensors: DHT Temperature, DHT Humidity, Home Temperature, Heater Water Temperature
- Actuators: Water Pump activation, Heater activation, Water Valve position
- System Health (displayed in binary — each bit maps to a task)

**Controls (visible after login only):**
- Heater ON / OFF toggle
- Target Home Temperature slider (5–30°C)
- Boiler Setpoint slider (40–70°C)
- Auto / Manual mode toggle

---

## 13. Git and Deploy Workflow

```
Dev branch:  DevelopmentASaab
Prod branch: main  (Render auto-deploys on every push)
```

**Commit and deploy in one command:**
```bash
git push origin DevelopmentASaab:main
```

**Before pushing to main — check:**
- `BoilerServerOnRTOS/index.js` → clientId must be `Server_Bridge_Main`
- `src/Secrets.h` must NOT be committed (gitignored)
- `BoilerServerOnRTOS/.env` must NOT be committed (gitignored)

---

## 14. Key Technical Decisions

| Decision | Reason |
|---|---|
| TaskCloud at priority 2 | `mqttClient.connect()` TLS handshake is blocking; at priority 1 round-robin context switches cause FPU stack overflow |
| TaskCloud stack = 400 words | TLS + JSON + String serialization needs deep stack; allocated first to guarantee contiguous heap block |
| HeaterTemp/HomeTemp stack = 150 words | FPU context save (18 words) on context switch during float operations; 100 words was too small |
| WDT.refresh() inside initializeCloud() | Init takes 15s+; worker tasks can't run while TaskCloud is blocking, so health bits never get set; direct WDT kicks bypass the health check during known-good init |
| vTaskDelay() instead of delay() | delay() busy-waits and does not yield to the RTOS scheduler; vTaskDelay() suspends the task and lets other tasks run |
| Retained MQTT status topic | UI always shows last known state immediately on page load, even before next 15s publish |
| Fixed MQTT clientIds | Prevents HiveMQ session stacking (duplicate sessions accumulating on each reconnect) |
| HEAT_LOSS_RATE = 1/86400 | Home equilibrates with outdoors in ~24h — realistic for an insulated house |
| HEAT_GAIN_RATE = 1/900 | Home reaches boiler temperature in ~15 min at full valve — realistic for underfloor/radiator heating |

---

## 15. Completed Work

| Feature | Status |
|---|---|
| FreeRTOS multi-task architecture | ✅ |
| Hardware WDT with health bitmask | ✅ |
| WDT init kicks during cloud initialisation | ✅ |
| Dual WiFi fallback | ✅ |
| MQTT TLS to HiveMQ + Last Will & Testament | ✅ |
| 10-minute offline hard reset | ✅ |
| HeaterModel — boiler water temperature simulation | ✅ |
| HomeTempModel — home air temperature simulation | ✅ |
| WaterActuator 3-zone proportional control with hysteresis | ✅ |
| Automatic thermostat (boiler setpoint with 5°C hysteresis) | ✅ |
| Manual override toggle (UI ↔ automatic) | ✅ |
| Open-Meteo real outdoor weather via Node.js MQTT injection | ✅ |
| Serial output cleaned (Cycle Stats block only) | ✅ |
| Code documentation (file paragraphs + function comments) | ✅ |
| Web UI dark theme, mobile responsive | ✅ |
| Login-gated controls | ✅ |

## 16. Planned Work

| Feature | Notes |
|---|---|
| Scheduled 2 AM daily reset | Use RTC (already NTP-synced) to call `NVIC_SystemReset()` at 02:00 — prevents long-term heap fragmentation and millis() drift |
| NVM/EEPROM persistence | Save `targetHomeTemp`, `heaterTempSetPoint`, `manualOverride` so settings survive a reboot |
| Real boiler water temperature sensor | Replace HeaterModel simulation with a physical sensor on the boiler circuit |
