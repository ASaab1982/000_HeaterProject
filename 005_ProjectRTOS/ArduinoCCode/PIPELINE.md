# Heater Project — Complete Pipeline

## Architecture Overview

```
Arduino UNO R4 WiFi
    |
    | MQTT over TLS (port 8883)
    | publishes every 15s to: boilers/B1/status
    | subscribes to: boilers/B1/commands
    v
HiveMQ Cloud Broker (Free Serverless)
    | d72dc8b632b04c8c91c4702a5b164d59.s1.eu.hivemq.cloud
    |
    | MQTT over TLS (port 8883)
    v
Node.js Bridge Server (Render.com)
    | https://zero00-heaterproject.onrender.com
    | Express + Socket.io + MQTT
    |
    | WebSocket (Socket.io)
    v
Browser / Mobile
    | index.html (dark UI)
    | Live sensor data + Heater controls
```

---

## Layer 1 — Arduino Firmware (FreeRTOS)

**Platform:** Arduino UNO R4 WiFi  
**Framework:** Arduino + FreeRTOS  
**Toolchain:** PlatformIO (VS Code)  
**Location:** `005_ProjectRTOS/src/`

### FreeRTOS Tasks (11 total)

| Task | File | Role |
|---|---|---|
| `TaskTimeScheduler` | Scheduler.cpp | Master clock — notifies all tasks every 250ms, cloud every 15s |
| `TaskDHT` | Sensors.cpp | Reads DHT11 temperature + humidity (pin 2) |
| `TaskThermistor` | Sensors.cpp | Reads analog thermistor (A0) |
| `TaskMic` | MicRead.cpp | Reads microphone ADC level (A1) |
| `TaskTouch` | TouchDisplay.cpp | Touch sensor via interrupt (pin 3) |
| `TaskDC` | DCMotor.cpp | Controls DC motor (pins 6/7/8) |
| `TaskStepper` | Stepper.cpp | Controls stepper motor (pins 10-13) |
| `TaskServo` | ServoControl.cpp | Controls servo motor (pin 5) |
| `TaskCloud` | DataToCloud.cpp | WiFi + MQTT connection to HiveMQ |
| `TaskHeapMonitor` | HeapMonitor.cpp | Monitors free RAM |
| `TaskWDTMon` | main.cpp | Kicks hardware watchdog (WDT) |

### Data Published to HiveMQ (every 15s)
```json
{
  "deviceId": "B1",
  "dhtTempC": 22.5,
  "dhtHumidity": 55,
  "thermistorTempC": 23.1,
  "dcMotorSpeed": 50,
  "stepperAngleDeg": 0,
  "servoPositionDeg": 0,
  "micAdc": 200,
  "touched": false,
  "systemHealth": 7,
  "heaterState": false,
  "targetHomeTemp": 20.0
}
```

### Commands Received from HiveMQ
```json
{ "command": "heater", "action": "on" }
{ "command": "heater", "action": "off" }
{ "command": "temperature", "value": 22.5 }
```

### Resilience Features
- **Hardware WDT** (5s timeout) — system resets if tasks freeze
- **Software watchdog task** — monitors system health byte
- **Dual WiFi fallback** — tries SSID1, then SSID2
- **MQTT Last Will & Testament** — HiveMQ publishes "offline" if Arduino drops
- **Hard reset** (NVIC_SystemReset) after 10 minutes offline

---

## Layer 2 — HiveMQ Cloud Broker

**Plan:** Free Serverless  
**Provider:** AWS (EU)  
**URL:** `d72dc8b632b04c8c91c4702a5b164d59.s1.eu.hivemq.cloud`  
**Port:** 8883 (TLS)  
**Limits:** 100 connections, 10 GB/month (~300 MB/month used = 3%)

### Topics
| Topic | Direction | Description |
|---|---|---|
| `boilers/B1/status` | Arduino → Node.js | Sensor telemetry (JSON, retained) |
| `boilers/B1/commands` | Node.js → Arduino | Heater commands (JSON) |

---

## Layer 3 — Node.js Bridge Server

**Platform:** Render.com (Free Web Service)  
**URL:** `https://zero00-heaterproject.onrender.com`  
**Location:** `005_ProjectRTOS/BoilerServerOnRTOS/`  
**Stack:** Express + Socket.io + MQTT.js

### Key Functions

| Function | Role |
|---|---|
| `io.on('connection')` | Sends cached boiler state to all new visitors immediately |
| `socket.on('login')` | Verifies credentials from .env, adds socket to 'authorized' room |
| `socket.on('heaterCommand')` | Forwards heater commands to HiveMQ (authorized users only) |
| `client.on('connect')` | Subscribes to boilers/B1/#, notifies UI bridge is up |
| `client.on('offline')` | Notifies UI bridge is disconnected |
| `client.on('reconnect')` | Notifies UI bridge is reconnecting |
| `client.on('error')` | Notifies UI of connection error |
| `client.on('message')` | Parses telemetry JSON and pushes to all browser clients |
| `setInterval (30s)` | Stale data watchdog — marks boiler as no-data after 90s silence |

### Environment Variables (stored on Render)
```
MQTT_USERNAME=<hivemq username>
MQTT_PASSWORD=<hivemq password>
WEB_USERNAME=<web login username>
WEB_PASSWORD=<web login password>
```

---

## Layer 4 — Web UI

**Location:** `005_ProjectRTOS/BoilerServerOnRTOS/public/index.html`  
**Stack:** HTML + CSS + Socket.io client

### Layout (2 columns)
```
┌──────────────────────────┬─────────────────────┐
│  [STATUS BANNER        ] │  [LOGIN BOX        ] │
│  🌡️ Boiler: HEALTHY      │                      │
│  🔗 Bridge: CONNECTED    │  [HEATER CONTROLS  ] │
│                          │  (after login only)  │
│  [BOILER DATA CARD     ] │                      │
│  DHT Temp, Humidity      │                      │
│  Thermistor, Motors...   │                      │
└──────────────────────────┴─────────────────────┘
```

### UI Behavior
- **No login required** → status banner + boiler data always visible
- **After login** → heater control card appears (ON/OFF + set temperature)
- **After logout** → control card hides, boiler data stays visible
- **Temperature input** → enforced 5–30°C on both UI and Arduino firmware

### Status Banner States
| Boiler Status | Color | Trigger |
|---|---|---|
| HEALTHY | 🟢 Green | Telemetry received |
| DISCONNECTED | 🔴 Red | Last Will & Testament |
| NO DATA | 🟠 Orange | No message for 90s |

| Bridge Status | Color | Trigger |
|---|---|---|
| CONNECTED | 🟢 Green | Node.js connected to HiveMQ |
| DISCONNECTED | 🔴 Red | Connection lost |
| RECONNECTING | 🟠 Orange | Trying to reconnect |
| ERROR | 🔴 Red | Connection error |

---

## Git Repository

**GitHub:** `https://github.com/ASaab1982/000_HeaterProject`  
**Main branch:** `main` (production — deployed on Render)  
**Dev branch:** `DevelopmentASaab` (development)

### Protected Files (gitignored)
- `src/Secrets.h` → WiFi + HiveMQ credentials for Arduino
- `BoilerServerOnRTOS/.env` → HiveMQ + web credentials for Node.js

---

## Next Steps
- [ ] Add boiler simulation function in Arduino (temperature rises/falls based on heaterState)
- [ ] Configure UptimeRobot to ping Render every 5 minutes (prevent sleep)
