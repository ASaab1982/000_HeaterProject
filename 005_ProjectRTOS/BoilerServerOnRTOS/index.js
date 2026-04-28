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

// This tells Node to serve the HTML/CSS from your public folder
app.use(express.static(path.join(__dirname, 'public')));

const brokerUrl = 'mqtts://d72dc8b632b04c8c91c4702a5b164d59.s1.eu.hivemq.cloud:8883';
const options = {
    clientId: 'Server_Bridge_Main', // Fixed ID to prevent session stacking
    username: process.env.MQTT_USERNAME,
    password: process.env.MQTT_PASSWORD,
    clean: true,                  // Cleans up the session when you disconnect
    connectTimeout: 4000,         // Wait 4 seconds for initial connection
    reconnectPeriod: 1000         // Try to reconnect every 1 second if offline
};

const client = mqtt.connect(brokerUrl, options);

// --- SOCKET AUTHENTICATION LOGIC ---
io.on('connection', (socket) => {
    console.log(`🔌 New connection: ${socket.id}`);

    socket.on('login', (creds) => {
        // Use credentials stored securely in the .env file
        if (creds.username === process.env.WEB_USERNAME && creds.password === process.env.WEB_PASSWORD) {
            console.log(`✅ ${socket.id} authenticated`);
            socket.join('authorized'); // Add this user to the authorized room
            socket.emit('loginResponse', { success: true });

            // Immediately push the last known state of all boilers to the new user
            Object.values(boilerStates).forEach(state => {
                socket.emit('boilerUpdate', state);
                // Also sync the visual status bar immediately on login
                socket.emit('boilerStatusUpdate', {
                    deviceId: state.deviceId,
                    status: state.connectionStatus || 'online'
                });
            });
        } else {
            console.log(`❌ ${socket.id} failed login`);
            socket.emit('loginResponse', { success: false, message: 'Invalid credentials' });
        }
    });

    // [2-WAY COMMUNICATION] Socket.io Command Listener: Receives 'heaterCommand' events from the web UI
    socket.on('heaterCommand', (commandData) => {
        if (socket.rooms.has('authorized')) { // Only authorized users can send commands
            const commandTopic = `boilers/${commandData.deviceId}/commands`;
            const payload = JSON.stringify(commandData.payload);
            // [2-WAY COMMUNICATION] MQTT Command Publishing: Relays the command to the Arduino via the HiveMQ broker
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

client.on('connect', () => {
    console.log('✅ Connected to HiveMQ');
    // Use a wildcard to capture status, telemetry, and any other sub-topics
    client.subscribe('boilers/B1/#');
});

client.on('message', (topic, message) => {
    const rawMessage = message.toString();
    const now = new Date();
    const timestamp = now.toLocaleTimeString() + `.${now.getMilliseconds()}`;

    // Initialize cache if it doesn't exist
    if (!boilerStates['B1']) boilerStates['B1'] = { deviceId: 'B1' };
    
    // Every time we get a message (Status OR JSON), update the last seen timer
    boilerStates['B1'].lastSeen = Date.now();

    // 1. HANDLE STATUS STRINGS (Last Will & Testament or Manual 'online' notice)
    if (rawMessage === 'online' || rawMessage === 'offline') {
        console.log(`\n📡 [STATUS] Alert at ${timestamp}`);
        console.log(`| Device: B1 | Status: ${rawMessage.toUpperCase()}`);
        console.log('---------------------------------------------------------');

        boilerStates['B1'].connectionStatus = rawMessage;

        // Notify UI specifically about the connection change
        io.to('authorized').emit('boilerStatusUpdate', {
            deviceId: 'B1',
            status: rawMessage,
            timestamp: timestamp
        });
        
        // Also send the full cached object to keep the UI in sync
        io.to('authorized').emit('boilerUpdate', boilerStates['B1']);
        return; 
    }

    // 2. HANDLE TELEMETRY DATA (JSON)
    try {
        const data = JSON.parse(rawMessage);
        
        if (data.deviceId) {
            // Merge new sensor data with the existing status (assumed online if sending data)
            boilerStates[data.deviceId] = { 
                ...data, 
                lastSeen: Date.now(),
                connectionStatus: 'online' 
            };
                    // --- Get the exact time ---
            const now = new Date();
            const timestamp = now.toLocaleTimeString() + `.${now.getMilliseconds()}`;

            // Log the detailed telemetry to the console
            console.log(`\n🐝 [HIVE] Device: ${data.deviceId || 'Unknown'} @ ${timestamp}`);
            console.log(`| DHT Temp: ${data.dhtTempC}°C | Humidity: ${data.dhtHumidity}% | Thermistor: ${data.thermistorTempC}°C`);
            console.log(`| Motor: ${data.dcMotorSpeed} | Stepper: ${data.stepperAngleDeg}° | Servo: ${data.servoPositionDeg}°`);
            console.log(`| Mic ADC: ${data.micAdc} | Touched: ${data.touched} | Health: 0x${Number(data.systemHealth).toString(16).toUpperCase()}`);
            // [2-WAY COMMUNICATION] Enhanced Logging: Logging the actual state reported back by the Arduino
            console.log(`| Heater: ${data.heaterState ? 'ON' : 'OFF'} | Target Home: ${data.targetHomeTemp}°C`);

            console.log('---------------------------------------------------------');

            // Push the merged data to the authorized Web UI
            io.to('authorized').emit('boilerUpdate', boilerStates[data.deviceId]);

            // CRITICAL FIX: If we just received telemetry, the system is clearly 'online'.
            // Tell the UI to turn the status bar green.
            io.to('authorized').emit('boilerStatusUpdate', {
                deviceId: data.deviceId,
                status: 'online'
            });
        }
        
    } catch (e) {
        // This catches malformed JSON or unexpected non-status strings
        console.log(`⚠️ [ERROR] Failed to parse message at ${timestamp}`);
        console.log(`| Raw Payload: ${rawMessage}`);
    }
});

// --- STALE DATA WATCHDOG ---
// Check every 30 seconds if we haven't heard from the boiler recently
setInterval(() => {
    const now = Date.now();
    const TIMEOUT = 90000; // 90 seconds threshold to detect the timeout

    Object.values(boilerStates).forEach(state => {
        // Only mark as 'no-data' if it's not already explicitly 'offline' (LWT)
        if (state.connectionStatus !== 'offline' && (now - (state.lastSeen || 0) > TIMEOUT)) {
            console.log(`⚠️  [WATCHDOG] No data from ${state.deviceId} for > 90s. Marking STALE.`);
            state.connectionStatus = 'no-data';
            
            io.to('authorized').emit('boilerStatusUpdate', {
                deviceId: state.deviceId,
                status: 'no-data'
            });
        }
    });
}, 30000); // the 30s is the check interval and the 90s is the timeout interval


// THIS IS THE KEY: It opens port 3000
server.listen(3000, () => {
    console.log('🚀 Server started! Open http://localhost:3000 in your browser');
});
