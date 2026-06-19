# Heater Project — System Resume

## Overview

ESP32-S3 based smart heating controller running FreeRTOS.  
The system reads the real home temperature from a **Xiaomi Mi 2 BLE sensor**, controls a boiler heater relay and a water pump/servo valve to maintain a user-defined target room temperature, and publishes telemetry to **HiveMQ** over TLS/MQTT every 15 seconds.

---

## Hardware

| Component | Pin / Interface |
|---|---|
| Heater relay | GPIO 11 |
| H-bridge pump — IN1 / IN2 / EN | GPIO 8 / 7 / 6 |
| Servo valve | GPIO 5 |
| DHT11 temperature/humidity | GPIO 2 |
| NTC water sensor (ADC) | A0 |
| Water flow sensor (ADC) | A1 |
| Xiaomi Mi 2 (LYWSD03MMC) | BLE — MAC `a4:c1:38:57:b9:f1` |

---

## Source Files

| File | Responsibility |
|---|---|
| `main.cpp` | Global variables, hardware init, task creation |
| `Scheduler.cpp` | Master 250 ms tick — notifies all tasks in sequence |
| `XiaomiBLE.cpp/h` | BLE GATT client — reads temperature from Xiaomi Mi 2 |
| `Heater.cpp/h` | Relay control + automatic thermostat logic |
| `HeaterModel.cpp/h` | Boiler water temperature simulation |
| `WaterActuator.cpp/h` | Pump + servo valve — 3-zone temperature control |
| `Sensors.cpp/h` | TaskHeaterTemp and TaskDHT wrappers |
| `DataToCloud.cpp/h` | WiFi / MQTT / NTP / telemetry / command parsing |
| `HeapMonitor.cpp/h` | Stack high-water marks and free heap logging |
| `Heater.h` | Heater task declarations |
| `DebugMacros.h` | `D_PRINT` / `D_PRINTLN` toggled by `DEBUG` define |
| `Secrets.h` | WiFi credentials, MQTT credentials, TLS CA certificate |
| `ProjectHeater.h` | Single umbrella include — all shared headers and externs |

---

## FreeRTOS Tasks

| Task | Core | Priority | Stack | Role |
|---|---|---|---|---|
| `TaskCloud` | 0 | 2 | 8192 | WiFi + MQTT telemetry + command receive |
| `TaskXiaomiBLE` | 0 | 1 | 4096 | BLE GATT connect → read home temp → disconnect |
| `TaskHeater` | 1 | 1 | 2048 | Heater relay + thermostat logic |
| `TaskWaterActuator` | 1 | 1 | 2048 | Water pump + servo valve |
| `TaskHeaterTemp` | 1 | 1 | 2048 | Boiler water temperature simulation |
| `TaskDHT` | 1 | 1 | 2048 | DHT11 humidity/temperature read |
| `TaskMonitor` | 1 | 1 | 2048 | Stack + heap diagnostics |
| `TaskTimeScheduler` | 1 | 3 | 2048 | Master tick — notifies all tasks |
| `TaskwatchdogMonitor` | 1 | 1 | 2048 | Hardware WDT kick |

**Core split rationale:** both radio tasks (WiFi and BLE) share the 2.4 GHz antenna on the ESP32-S3.  
Pinning them to CPU 0 keeps them aligned with the ESP-IDF WiFi/BLE stack and isolated from the deterministic 250 ms control loop on CPU 1.

---

## Scheduler Timing

```
Each cycle = 7 slots × 250 ms = 1.75 s

Slot 1  +0 ms    TaskHeater
Slot 2  +250 ms  TaskWaterActuator
Slot 3  +500 ms  TaskHeaterTemp
Slot 4  +750 ms  TaskDHT
Slot 5  +1000 ms TaskCloud        (fired only every 5 min)
Slot 6  +1250 ms TaskMonitor
Slot 7  +1500 ms TaskwatchdogMonitor

+ TaskXiaomiBLE  notified once, exactly 2 min after each TaskCloud fire
```

The 2-minute gap between Cloud and XiaomiBLE lets the MQTT transaction fully complete before BLE coexistence begins.

---

## Home Temperature — Xiaomi Mi 2 BLE

The sensor runs **original Xiaomi firmware** (no custom flash needed).  
Connection strategy per cycle:
1. BLE GATT connect to `XIAOMI_SENSOR_MAC`
2. 1 s settle delay (GATT discovery)
3. Subscribe to NOTIFY on characteristic `EBE0CCC1-…` — wait up to 10 s
4. Fallback: direct `readValue()` if no notification arrives
5. Disconnect — 500 ms settle

On success: `g_homeTemp` and `g_xiaomiHumidity` are updated directly.  
On failure / stale: `g_homeTemp` holds its last known value — the thermostat continues safely.

---

## Thermostat Control (WaterActuator)

3-zone logic with ±0.5 °C deadband and 0.2 °C hysteresis:

| Zone | Condition | Action |
|---|---|---|
| Too cold | `g_homeTemp < target − 0.6 °C` | Pump ON, valve fully open (180°) |
| Equilibrium | within ±0.4 °C of target | Pump ON, valve at computed balance angle |
| Too warm | `g_homeTemp > target + 0.6 °C` | Pump OFF, valve fully closed (0°) |

Equilibrium valve angle = analytical solution where boiler heat gain equals outdoor heat loss.

---

## Cloud (HiveMQ over TLS)

- **Broker:** `d72dc8b632b04c8c91c4702a5b164d59.s1.eu.hivemq.cloud:8883`
- **Telemetry topic:** `boilers/ESP/status` — JSON payload every 15 s
- **Command topic:** `boilers/ESP/commands` — receives: heater on/off, target temp, manual override, boiler setpoint, outdoor weather (Open-Meteo via Node.js)
- **Fallback:** dual WiFi SSIDs; 10-minute MQTT outage triggers `esp_restart()`
- **TLS:** Let's Encrypt YR2 intermediate CA — next renewal ~Sep 2026

---

## Watchdog

Hardware WDT timeout: **5 seconds**.  
`TaskwatchdogMonitor` kicks it only when `systemHealth == 0x3D`:

| Bit | Task |
|---|---|
| 0 | WaterActuator pump |
| 2 | HeaterTemp |
| 3 | DHT |
| 4 | Heater |
| 5 | WaterActuator valve |

`systemHealth` is reset to `0x00` every watchdog cycle regardless of outcome.

---

## Key Global Variables

| Variable | Type | Description |
|---|---|---|
| `g_homeTemp` | `volatile float` | Real home temperature — written by TaskXiaomiBLE |
| `g_xiaomiHumidity` | `volatile float` | Home humidity from Xiaomi sensor |
| `g_heaterWaterTemp` | `volatile float` | Simulated boiler water temperature |
| `g_outdoorTemp` | `volatile float` | Outdoor temp — from Open-Meteo via MQTT |
| `targetHomeTemp` | `volatile float` | User setpoint (default 20 °C) |
| `heaterTempSetPoint` | `volatile float` | Boiler water setpoint (default 40 °C) |
| `heaterState` | `volatile bool` | Current heater relay state |
| `manualOverride` | `volatile bool` | true = UI controls heater directly |
| `g_useCloudWeather` | `volatile bool` | true = use Open-Meteo data, skip DHT |
| `systemHealth` | `volatile byte` | Bitmask — all bits set = system healthy |
