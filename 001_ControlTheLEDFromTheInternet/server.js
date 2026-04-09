const express = require("express");
const path = require("path");

const app = express();
const PORT = process.env.PORT || 3000;

app.use(express.json());
app.use(express.static(path.join(__dirname, "public")));

// In-memory state for quick prototype.
// This is enough for testing with one Arduino + one service instance.
let ledState = "off";
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

app.get("/api/led", (req, res) => {
  res.json({
    state: ledState
  });
});

app.post("/api/led", requireAuth, (req, res) => {
  const { state } = req.body || {};
  if (state !== "on" && state !== "off") {
    return res.status(400).json({
      ok: false,
      error: "state must be 'on' or 'off'"
    });
  }

  ledState = state;
  return res.json({
    ok: true,
    state: ledState
  });
});

app.get("*", (req, res) => {
  res.sendFile(path.join(__dirname, "public", "index.html"));
});

app.listen(PORT, () => {
  console.log(`Server running on port ${PORT}`);
});
