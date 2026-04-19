require('dotenv').config(); // <--- CRITICAL: This must be line 1
const express = require("express");
const path = require("path");

const app = express();
const PORT = process.env.PORT || 3000;


app.use(express.json());
app.use(express.static(path.join(__dirname, "Public")));

// Middleware to log all incoming requests
app.use((req, res, next) => {
  console.log(`[${new Date().toLocaleTimeString()}] ${req.method} ${req.url}`);
  if (Object.keys(req.body).length > 0) {
    console.log("Payload:", JSON.stringify(req.body, null, 2));
  }
  next();
});


const telemetry = {
  dcMotorSpeed: 0,
  micAdc: 0,
  thermistorTempC: 0,
  dhtTempC: 0,
  dhtHumidity: 0,
  stepperAngleDeg: 0,
  servoPositionDeg: 0,
  touched: false,
  freeHeap: 0,     // <--- New
  minHeap: 0,      // <--- New
  updatedAt: new Date().toISOString()
};

const sessions = new Set();
const WEB_USERNAME = process.env.WEB_USERNAME ;
const WEB_PASSWORD = process.env.WEB_PASSWORD ;
const ARDUINO_STATIC_TOKEN = process.env.ARDUINO_TOKEN
sessions.add(ARDUINO_STATIC_TOKEN); // Pre-authorize the Arduino

console.log("🔐 [SYSTEM]: Static Arduino Token loaded into session.");

function requireAuth(req, res, next) {
  const authHeader = req.header("Authorization") || "";
  const [scheme, token] = authHeader.split(" ");
  
  if (scheme !== "Bearer" || !token || !sessions.has(token)) {
    console.log("❌ [AUTH FAILED]: Invalid or missing token."); // ADD THIS LINE
    return res.status(401).json({ ok: false, error: "Unauthorized" });
  }
  
  console.log("✅ [AUTH SUCCESS]: Token accepted."); // ADD THIS LINE
  return next();
}

app.post("/api/login", (req, res) => {
  const { username, password } = req.body || {};
  if (username !== WEB_USERNAME || password !== WEB_PASSWORD) {
    return res.status(401).json({ ok: false, error: "Invalid credentials" });
  }

  const token = Math.random().toString(36).slice(2) + Date.now().toString(36);
  sessions.add(token);
  return res.json({ ok: true, token });
});

app.get("/api/telemetry", requireAuth, (req, res) => {
  return res.json({ ok: true, telemetry });
});

app.post("/api/telemetry", requireAuth, (req, res) => {
  const body = req.body || {};

  const nextTelemetry = {
    // If body.dcMotorSpeed is missing, use the current telemetry value
    dcMotorSpeed: Number(body.dcMotorSpeed ?? telemetry.dcMotorSpeed),
    micAdc: Number(body.micAdc ?? telemetry.micAdc),
    thermistorTempC: Number(body.thermistorTempC ?? telemetry.thermistorTempC),
    dhtTempC: Number(body.dhtTempC ?? telemetry.dhtTempC),
    dhtHumidity: Number(body.dhtHumidity ?? telemetry.dhtHumidity),
    stepperAngleDeg: Number(body.stepperAngleDeg ?? telemetry.stepperAngleDeg),
    servoPositionDeg: Number(body.servoPositionDeg ?? telemetry.servoPositionDeg),
    touched: body.touched !== undefined ? Boolean(body.touched) : telemetry.touched,
    freeHeap: Number(body.freeHeap ?? telemetry.freeHeap), // <--- Added
    minHeap: Number(body.minHeap ?? telemetry.minHeap),   // <--- Added
    updatedAt: new Date().toISOString()
  };

  const numericKeys = [
    "dcMotorSpeed",
    "micAdc",
    "thermistorTempC",
    "dhtTempC",
    "dhtHumidity",
    "stepperAngleDeg",
    "servoPositionDeg",
    "freeHeap", 
    "minHeap"   
  ];

  for (const key of numericKeys) {
    if (!Number.isFinite(nextTelemetry[key])) {
      return res.status(400).json({
        ok: false,
        error: `Invalid numeric value for '${key}'`
      });
    }
  }

  Object.assign(telemetry, nextTelemetry);
  return res.json({ ok: true, telemetry });
});

app.get("*", (req, res) => {
  res.sendFile(path.join(__dirname, "Public", "index.html"));
});

app.listen(PORT, () => {
  console.log(`Server running on port ${PORT}`);
});
