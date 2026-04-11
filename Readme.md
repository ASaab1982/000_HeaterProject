# Heater Project - Arduino UNO R4 WiFi

## Goal
Build a heater-monitoring and control system on Arduino UNO R4 WiFi using FreeRTOS and Arduino libraries.

Current phase: component selection and PCB preparation.  
Future phase: IoT connection (AWS host).

## Platform
- Board: Arduino UNO R4 WiFi
- IDE: Arduino IDE
- RTOS approach: FreeRTOS with Arduino ecosystem
- Cloud step (later): AWS IoT integration

## Confirmed Components

### GPIO Components
- 28BYJ-48 stepper motor with ULN2003 driver
- Servo motor (PWM position control)
- DC motor with L293D driver (direction + speed control)
- DHT11 temperature/humidity sensor
- TTP223 touch sensor

### Analog / Display Components
- Thermistor temperature sensor (resistance details to be added later)
- LCD1602 with I2C interface
- Microphone sensor with analog output (AO)

## Agreed Pin Map
- DHT11 -> D2
- TTP223 -> D3
- Servo PWM -> D5
- L293D EN (PWM speed) -> D6
- L293D IN1 -> D7
- L293D IN2 -> D8
- ULN2003 IN1 -> D9
- ULN2003 IN2 -> D10
- ULN2003 IN3 -> D11
- ULN2003 IN4 -> D12
- Thermistor -> A0
- Microphone AO -> A1
- LCD1602 I2C -> SDA / SCL

## Power Architecture Decision
- Motors are powered by an external 5V battery/supply.
- Arduino logic power comes from a separate AC/DC converter.
- Grounds must be connected together (common reference ground):
  - Motor supply GND
  - Driver GND
  - Arduino GND

Reason: control signals are interpreted relative to ground; without common ground, logic levels can be unstable or invalid.

## Firmware Architecture (Planned)
- TaskSensors: DHT11, thermistor, microphone, touch sensor
- TaskActuators: servo, DC motor, stepper motor
- TaskUI: LCD updates
- TaskLogic: conditions/state machine
- TaskComms (later): AWS IoT communication

## Phased Roadmap
1. Finalize schematic and PCB (power domains, connectors, grounding, labels)
2. Bring up firmware basics with RTOS task skeleton
3. Integrate sensors and local display
4. Integrate actuators and decision logic
5. Add WiFi/AWS communication

## Session Notes
- Work is done step by step; no large code dump unless requested.
- Current focus remains PCB and hardware reliability first.

## Today Progress (Apr 6, 2026)
- Reviewed and validated sketch structure in `003_SM_DC_Servo` and `004_CompleteProject`.
- Refactored actuator and sensor logic out of the main `.ino` into separate C++ modules in `004_CompleteProject`:
  - `StepperControl.h/.cpp`
  - `DCMotorControl.h/.cpp`
  - `ServoControl.h/.cpp`
  - `SensorReadings.h/.cpp`
- Updated `004_CompleteProject.ino` to keep `setup()` and `loop()` as orchestrator code and call functions from the new modules.
- Kept ISR `handleTouchInterrupt()` in the main sketch and routed touch display through module function.
- Confirmed FreeRTOS availability on UNO R4 WiFi through `Arduino_FreeRTOS.h`; `task.h` is included internally by that header.

## Next Session Plan (FreeRTOS Migration)
- Convert to task-based architecture with at least three tasks:
  - TaskStepper
  - TaskDCMotor
  - TaskServo
- Add sensor task integration (thermistor, DHT11, microphone, touch) and shared-state strategy.
- Replace blocking `delay()` patterns with RTOS-safe timing (`vTaskDelay` / tick-based scheduling).
- Define task priorities, stack sizes, and safe communication primitives (queues/mutex if needed).

## Review Update (Apr 8, 2026) - `005_ProjectRTOS`
- Reviewed folder `005_ProjectRTOS` structure and module split:
  - Main sketch: `005_ProjectRTOS.ino`
  - Modules: `Stepper`, `DCMotor`, `Servo`, `Sensors`, `MicRead`, `TouchDisplay`
- Current RTOS architecture is task-per-function with task notifications:
  - `TaskMasterTimer` triggers worker tasks every second in a 10-second cycle
  - Worker tasks block on `ulTaskNotifyTake(...)` and run when notified
- Positive points:
  - Runtime task timing mostly uses `vTaskDelay(...)`
  - Monitoring task prints stack high-water marks and heap usage
  - Clear modular organization for actuators and sensors
- Risks/issues observed:
  - Wi-Fi credentials are currently hardcoded in source
  - Touch interrupt can fire before `hTouch` is created, so ISR notification should be guarded
  - Header consistency cleanup needed (`enable1Pin` vs `enablePin`, include guard style)
  - Local file name `Servo.h` may conflict with Arduino Servo library naming
- Recommended next hardening pass:
  - Guard ISR notify with handle null-check logic
  - Normalize header guards in all module headers
  - Rename local servo module files to avoid naming collision (for example `ServoAction.h/.cpp`)
  - Move credentials to a separate local config file excluded from version control

## Session Log (Apr 10, 2026) - RTOS Telemetry + Web Dashboard
- Defined shared telemetry globals in `005_ProjectRTOS.ino` for Wi-Fi publishing:
  - `g_dcMotorSpeed`
  - `g_micAdc`
  - `g_thermistorTempC`
  - `g_dhtTempC`
  - `g_dhtHumidity`
  - `g_stepperAngleDeg`
  - `g_servoPositionDeg`
  - `touched`
- Added matching `extern` declarations in `005_ProjectRTOS/ProjectHeater.h` so values are accessible across modules.
- Updated module files to write telemetry values:
  - `DCMotor.cpp` updates DC speed
  - `MicRead.cpp` updates mic ADC
  - `Sensors.cpp` updates thermistor and DHT values
  - `ServoControl.cpp` updates servo position
- Notes kept during session:
  - `extern` is declaration across files; real definition exists in one file only.
  - `volatile` is used for shared/async-updated values (tasks/ISR visibility), but it is not a lock.
  - `const` means read-only after initialization; can be shared with `extern const`.
- Created standalone authenticated telemetry web app in the main project folder:
  - Folder: `006_TelemetryWebServer`
  - Files:
    - `006_TelemetryWebServer/server.js`
    - `006_TelemetryWebServer/Public/index.html`
    - `006_TelemetryWebServer/package.json`
    - `006_TelemetryWebServer/README.md`
- Server/API behavior:
  - `POST /api/login` returns bearer token
  - `GET /api/telemetry` returns telemetry (auth required)
  - `POST /api/telemetry` updates telemetry (auth required)
  - In-memory storage starts with zero/default values until Arduino posts data
- UI behavior:
  - Login/logout + periodic refresh
  - Dashboard boxes display all telemetry fields listed above
  - `Last update` shows backend timestamp

## Next Session Quick Start
1. Open `006_TelemetryWebServer`
2. Run:
   - `npm install`
   - `npm start`
3. Open `http://localhost:3000`
4. Login with default credentials (`admin` / `admin123`) or env-configured credentials
5. Connect Arduino HTTP client to:
   - `POST /api/login` (get token)
   - `POST /api/telemetry` (send live values)