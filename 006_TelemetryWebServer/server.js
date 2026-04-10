const express = require("express");
const path = require("path");

const app = express();
const PORT = process.env.PORT || 3000;

app.use(express.json());
app.use(express.static(path.join(__dirname, "Public")));

const telemetry = {
  dcMotorSpeed: 0,
  micAdc: 0,
  thermistorTempC: 0,
  dhtTempC: 0,
  dhtHumidity: 0,
  stepperAngleDeg: 0,
  servoPositionDeg: 0,
  touched: false,
  updatedAt: new Date().toISOString()
};

const sessions = new Set();
const WEB_USERNAME = process.env.WEB_USERNAME || "admin";
const WEB_PASSWORD = process.env.WEB_PASSWORD || "admin123";

function requireAuth(req, res, next) {
  const authHeader = req.header("Authorization") || "";
  const [scheme, token] = authHeader.split(" ");
  if (scheme !== "Bearer" || !token || !sessions.has(token)) {
    return res.status(401).json({ ok: false, error: "Unauthorized" });
  }
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
    dcMotorSpeed: Number(body.dcMotorSpeed),
    micAdc: Number(body.micAdc),
    thermistorTempC: Number(body.thermistorTempC),
    dhtTempC: Number(body.dhtTempC),
    dhtHumidity: Number(body.dhtHumidity),
    stepperAngleDeg: Number(body.stepperAngleDeg),
    servoPositionDeg: Number(body.servoPositionDeg),
    touched: Boolean(body.touched),
    updatedAt: new Date().toISOString()
  };

  const numericKeys = [
    "dcMotorSpeed",
    "micAdc",
    "thermistorTempC",
    "dhtTempC",
    "dhtHumidity",
    "stepperAngleDeg",
    "servoPositionDeg"
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
