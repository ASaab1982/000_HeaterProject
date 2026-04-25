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
    client.subscribe('boilers/B1/status');
});

client.on('message', (topic, message) => {
    try {
        const data = JSON.parse(message.toString());
        
        // Save the data to our cache so it can be sent to users who log in later
        if (data.deviceId) {
            boilerStates[data.deviceId] = data;
        }
        
        // --- Get the exact time ---
        const now = new Date();
        const timestamp = now.toLocaleTimeString() + `.${now.getMilliseconds()}`;

        // --- 1. DATA RECEIVED FROM HIVE ---
        console.log(`\n🐝 [HIVE] Device: ${data.deviceId || 'Unknown'} @ ${timestamp}`);
        console.log(`| DHT Temp: ${data.dhtTempC}°C | Humidity: ${data.dhtHumidity}% | Thermistor: ${data.thermistorTempC}°C`);
        console.log(`| Motor: ${data.dcMotorSpeed} | Stepper: ${data.stepperAngleDeg}° | Servo: ${data.servoPositionDeg}°`);
        console.log(`| Mic ADC: ${data.micAdc} | Touched: ${data.touched} | Health: 0x${Number(data.systemHealth).toString(16).toUpperCase()}`);
        // [2-WAY COMMUNICATION] Enhanced Logging: Logging the actual state reported back by the Arduino
        console.log(`| Heater: ${data.heaterState ? 'ON' : 'OFF'} | Target Home: ${data.targetHomeTemp}°C`);

        console.log('---------------------------------------------------------');

        // --- 2. DATA PUSHED TO UI ---
        // Only send updates to sockets in the 'authorized' room
        io.to('authorized').emit('boilerUpdate', data); 
        console.log(`🖥️ [UI] Broadcasted at ${new Date().toLocaleTimeString()}`);
        
    } catch (e) {
        console.log(`⚠️ [${new Date().toLocaleTimeString()}] Non-JSON: ${message.toString()}`);
    }
});

// THIS IS THE KEY: It opens port 3000
server.listen(3000, () => {
    console.log('🚀 Server started! Open http://localhost:3000 in your browser');
});