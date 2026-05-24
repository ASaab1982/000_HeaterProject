const mqtt = require('mqtt');
const express = require('express');
const http = require('http');
const { Server } = require('socket.io');
const path = require('path');
require('dotenv').config();

const app = express();
const server = http.createServer(app);
const io = new Server(server);

const boilerStates = {}; // Cache the latest data for each boiler to persist across UI refreshes

// [WEATHER] Coordinates for Neirivue, Canton de Fribourg, Switzerland
const WEATHER_LAT = 46.52769;
const WEATHER_LON = 7.06050;
// [WEATHER] Prevents stacking multiple intervals if MQTT reconnects
let weatherInterval = null;

// [WEATHER] Fetches current outdoor temperature and humidity from Open-Meteo (free, no API key)
// and publishes them to the Arduino via the existing MQTT commands topic.
// The Arduino will use these values to replace its physical DHT sensor readings.
async function fetchWeatherData() {
    try {
        const url = `https://api.open-meteo.com/v1/forecast?latitude=${WEATHER_LAT}&longitude=${WEATHER_LON}&current=temperature_2m,relative_humidity_2m`;
        const response = await fetch(url);
        const json = await response.json();
        const temp = json.current.temperature_2m;
        const humidity = json.current.relative_humidity_2m;

        const payload = JSON.stringify({ command: 'weather', outdoorTemp: temp, outdoorHumidity: humidity });
        // [WEATHER] Published to a dedicated retained topic so the Arduino receives it
        // immediately on subscribe, regardless of when it connects relative to Node.js.
        // retain: true means HiveMQ stores the last value — no timing race on Arduino reboot.
        client.publish('boilers/B1/weather', payload, { qos: 0, retain: true }, (error) => {
            if (error) {
                console.error('❌ [WEATHER] Failed to publish weather data:', error);
            } else {
                console.log(`🌤️  [WEATHER] Neirivue: ${temp}°C, ${humidity}% humidity → sent to Arduino`);
            }
        });
    } catch (err) {
        console.error('❌ [WEATHER] Failed to fetch from Open-Meteo:', err.message);
    }
}

// Serves the HTML/CSS/JS files from the 'public' folder to the browser
app.use(express.static(path.join(__dirname, 'public')));

const brokerUrl = 'mqtts://d72dc8b632b04c8c91c4702a5b164d59.s1.eu.hivemq.cloud:8883';
const options = {
    clientId: 'Server_Bridge_Local', // Fixed ID to prevent session stacking
    username: process.env.MQTT_USERNAME,
    password: process.env.MQTT_PASSWORD,
    clean: true,                  // Cleans up the session when you disconnect
    connectTimeout: 4000,         // Wait 4 seconds for initial connection
    reconnectPeriod: 1000         // Try to reconnect every 1 second if offline
};

const client = mqtt.connect(brokerUrl, options);

// --- SOCKET.IO: NEW BROWSER CONNECTION ---
// Fires every time a new browser opens the page.
// Immediately sends the latest cached boiler data and bridge status to the new visitor,
// so they see live data right away without waiting for the next Arduino update.
io.on('connection', (socket) => {
    console.log(`🔌 New connection: ${socket.id}`);

    Object.values(boilerStates).forEach(state => {
        socket.emit('boilerUpdate', state);
        socket.emit('boilerStatusUpdate', {
            deviceId: state.deviceId,
            status: state.connectionStatus || 'online'
        });
    });

    socket.emit('bridgeStatus', { status: client.connected ? 'connected' : 'disconnected' });

    // --- SOCKET.IO: LOGIN ---
    // Receives username and password from the browser login form.
    // Checks credentials against the .env file.
    // If valid: adds the socket to the 'authorized' room, which unlocks heater controls.
    // If invalid: sends back an error message to the browser.
    socket.on('login', (creds) => {
        if (creds.username === process.env.WEB_USERNAME && creds.password === process.env.WEB_PASSWORD) {
            console.log(`✅ ${socket.id} authenticated`);
            socket.join('authorized');
            socket.emit('loginResponse', { success: true });
        } else {
            console.log(`❌ ${socket.id} failed login`);
            socket.emit('loginResponse', { success: false, message: 'Invalid credentials' });
        }
    });

    // --- SOCKET.IO: HEATER COMMAND ---
    // Receives heater control commands from the browser (heater ON/OFF or set temperature).
    // Only executes if the socket is in the 'authorized' room (user is logged in).
    // Publishes the command as a JSON message to HiveMQ on the topic boilers/<deviceId>/commands,
    // which the Arduino is subscribed to and will execute immediately.
    socket.on('heaterCommand', (commandData) => {
        if (socket.rooms.has('authorized')) {
            const commandTopic = `boilers/${commandData.deviceId}/commands`;
            const payload = JSON.stringify(commandData.payload);
            client.publish(commandTopic, payload, { qos: 0, retain: false }, (error) => {
                if (error) {
                    console.error(`❌ [MQTT] Failed to publish command to ${commandTopic}:`, error);
                } else {
                    console.log(`⬆️ [MQTT] Command published to ${commandTopic}: ${payload}`);
                }
            });
        }
    });
});

// --- MQTT: CONNECTED TO HIVEMQ ---
// Fires when the Node.js server successfully connects to the HiveMQ broker.
// Subscribes to all topics under boilers/B1/ using a wildcard (#).
// Notifies all browser clients that the bridge link is up (green indicator).
client.on('connect', () => {
    console.log('✅ Connected to HiveMQ');
    client.subscribe('boilers/B1/#');
    io.emit('bridgeStatus', { status: 'connected' });
    // [WEATHER] Fetch immediately on connect so Arduino has outdoor data right away,
    // then refresh every 10 minutes (outdoor temp doesn't change faster than that)
    fetchWeatherData();
    if (!weatherInterval) {
        weatherInterval = setInterval(fetchWeatherData, 10 * 60 * 1000);
    }
});

// --- MQTT: BRIDGE WENT OFFLINE ---
// Fires when the Node.js server loses its connection to HiveMQ.
// Notifies all browser clients so the bridge indicator turns red.
client.on('offline', () => {
    console.log('⚠️  [BRIDGE] Lost connection to HiveMQ. Reconnecting...');
    io.emit('bridgeStatus', { status: 'disconnected' });
});

// --- MQTT: BRIDGE IS RECONNECTING ---
// Fires on every reconnection attempt after a lost connection.
// Notifies all browser clients so the bridge indicator turns orange.
client.on('reconnect', () => {
    console.log('🔄 [BRIDGE] Reconnecting to HiveMQ...');
    io.emit('bridgeStatus', { status: 'reconnecting' });
});

// --- MQTT: BRIDGE ERROR ---
// Fires when a connection error occurs (wrong credentials, network issue, etc.).
// Logs the error and notifies all browser clients so the bridge indicator turns red.
client.on('error', (err) => {
    console.error('❌ [BRIDGE] HiveMQ error:', err.message);
    io.emit('bridgeStatus', { status: 'error' });
});

// --- MQTT: MESSAGE RECEIVED FROM HIVEMQ ---
// Fires every time a message arrives on any subscribed topic (boilers/B1/#).
// Handles two types of messages:
//   1. Status strings: "online" or "offline" — sent by Arduino's Last Will & Testament.
//      Updates the boiler connection status and notifies the browser immediately.
//   2. JSON telemetry: sensor data from the Arduino (temp, humidity, motors, etc.).
//      Merges new data into the cache and pushes it to all browser clients.
client.on('message', (topic, message) => {
    const rawMessage = message.toString();
    const now = new Date();
    const timestamp = now.toLocaleTimeString() + `.${now.getMilliseconds()}`;

    if (!boilerStates['B1']) boilerStates['B1'] = { deviceId: 'B1' };
    boilerStates['B1'].lastSeen = Date.now();

    // 1. HANDLE STATUS STRINGS (Last Will & Testament or Manual 'online' notice)
    if (rawMessage === 'online' || rawMessage === 'offline') {
        console.log(`\n📡 [STATUS] Alert at ${timestamp}`);
        console.log(`| Device: B1 | Status: ${rawMessage.toUpperCase()}`);
        console.log('---------------------------------------------------------');

        boilerStates['B1'].connectionStatus = rawMessage;

        io.emit('boilerStatusUpdate', {
            deviceId: 'B1',
            status: rawMessage,
            timestamp: timestamp
        });

        io.emit('boilerUpdate', boilerStates['B1']);
        return;
    }

    // 2. HANDLE TELEMETRY DATA (JSON)
    try {
        const data = JSON.parse(rawMessage);

        if (data.deviceId) {
            boilerStates[data.deviceId] = {
                ...data,
                lastSeen: Date.now(),
                connectionStatus: 'online'
            };

            const now = new Date();
            const timestamp = now.toLocaleTimeString() + `.${now.getMilliseconds()}`;

            console.log(`\n🐝 [HIVE] Device: ${data.deviceId || 'Unknown'} @ ${timestamp}`);
            console.log(`| DHT Temp: ${data.dhtTempC}°C | Humidity: ${data.dhtHumidity}% | Home: ${data.homeTempC}°C`);
            console.log(`| WaterPump: ${data.waterpumpactivation} | Heater: ${data.heaterActivation} | WaterValve: ${data.waterValvePosition}`);
            console.log(`| Heater Water Temp: ${data.heaterWaterTemp}°C | Health: 0x${Number(data.systemHealth).toString(16).toUpperCase()}`);
            console.log(`| Heater: ${data.heaterState ? 'ON' : 'OFF'} | Target Home: ${data.targetHomeTemp}°C`);
            console.log('---------------------------------------------------------');

            io.emit('boilerUpdate', boilerStates[data.deviceId]);
            io.emit('boilerStatusUpdate', {
                deviceId: data.deviceId,
                status: 'online'
            });
        }

    } catch (e) {
        console.log(`⚠️ [ERROR] Failed to parse message at ${timestamp}`);
        console.log(`| Raw Payload: ${rawMessage}`);
    }
});

// --- STALE DATA WATCHDOG ---
// Runs every 30 seconds. Checks if any boiler has stopped sending data for more than 90 seconds.
// If yes: marks it as 'no-data' and notifies the browser (orange banner).
// This catches the case where the Arduino freezes or loses WiFi without a clean disconnect.
setInterval(() => {
    const now = Date.now();
    const TIMEOUT = 90000;

    Object.values(boilerStates).forEach(state => {
        if (state.connectionStatus !== 'offline' && (now - (state.lastSeen || 0) > TIMEOUT)) {
            console.log(`⚠️  [WATCHDOG] No data from ${state.deviceId} for > 90s. Marking STALE.`);
            state.connectionStatus = 'no-data';

            io.emit('boilerStatusUpdate', {
                deviceId: state.deviceId,
                status: 'no-data'
            });
        }
    });
}, 30000);

// --- START SERVER ---
// Opens the HTTP server on the port assigned by the platform (Render), or 3000 locally.
// Socket.io and Express both share this same server instance.
const PORT = process.env.PORT || 3000;
server.listen(PORT, () => {
    console.log(`🚀 Server started! Open http://localhost:${PORT} in your browser`);
});
