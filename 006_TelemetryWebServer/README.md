# Heater Telemetry Web Server

This folder contains a standalone authenticated Node.js server and HTML page to display telemetry from your Arduino RTOS variables:

- `g_dcMotorSpeed`
- `g_micAdc`
- `g_thermistorTempC`
- `g_dhtTempC`
- `g_dhtHumidity`
- `g_stepperAngleDeg`
- `g_servoPositionDeg`
- `touched`

## Run

```bash
npm install
npm start
```

Open `http://localhost:3000`.

## Authentication

Default credentials:

- Username: `admin`
- Password: `admin123`

Override with environment variables:

- `WEB_USERNAME`
- `WEB_PASSWORD`

## API

- `POST /api/login` -> returns bearer token
- `GET /api/telemetry` -> read telemetry (auth required)
- `POST /api/telemetry` -> update telemetry (auth required)

Sample `POST /api/telemetry` body:

```json
{
  "dcMotorSpeed": 50,
  "micAdc": 321,
  "thermistorTempC": 27.4,
  "dhtTempC": 26.9,
  "dhtHumidity": 47.2,
  "stepperAngleDeg": 90.0,
  "servoPositionDeg": 45,
  "touched": false
}
```
