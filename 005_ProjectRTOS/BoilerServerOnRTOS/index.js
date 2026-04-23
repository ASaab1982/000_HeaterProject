const mqtt = require('mqtt');
const express = require('express');
const http = require('http');
const { Server } = require('socket.io');
const path = require('path');
require('dotenv').config();

const app = express();
const server = http.createServer(app);
const io = new Server(server);

// This tells Node to serve the HTML/CSS from your public folder
app.use(express.static(path.join(__dirname, 'public')));

const brokerUrl = 'mqtts://d72dc8b632b04c8c91c4702a5b164d59.s1.eu.hivemq.cloud:8883';
const options = {
    clientId: 'Server_Bridge_' + Math.random().toString(16).substring(2, 8),
    username: process.env.MQTT_USERNAME,
    password: process.env.MQTT_PASSWORD
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
        } else {
            console.log(`❌ ${socket.id} failed login`);
            socket.emit('loginResponse', { success: false, message: 'Invalid credentials' });
        }
    });
});

client.on('connect', () => {
    console.log('✅ Connected to HiveMQ');
    // Subscribe to ALL boilers using the '+' wildcard
    client.subscribe('boilers/+/status');
});

client.on('message', (topic, message) => {
    try {
        const data = JSON.parse(message.toString());
        
        // Extract Boiler ID from topic (e.g., "boilers/B1/status" -> "B1")
        const topicParts = topic.split('/');
        data.boilerId = topicParts[1]; 

        // --- Get the exact time ---
        const now = new Date();
        const timestamp = now.toLocaleTimeString() + `.${now.getMilliseconds()}`;

        // --- 1. DATA RECEIVED FROM HIVE ---
        console.log(`\n🐝 [HIVE] @ ${timestamp} | Boiler: ${data.boilerId}`);
        console.log(`| DHT Temp: ${data.dhtTempC}°C | Humidity: ${data.dhtHumidity}% | Thermistor: ${data.thermistorTempC}°C`);
        console.log(`| Motor: ${data.dcMotorSpeed} | Stepper: ${data.stepperAngleDeg}° | Servo: ${data.servoPositionDeg}°`);
        console.log(`| Mic ADC: ${data.micAdc} | Touched: ${data.touched} | Health: 0x${Number(data.systemHealth).toString(16).toUpperCase()}`);
        console.log('---------------------------------------------------------');

        // --- 2. DATA PUSHED TO UI ---
        // Only send updates to sockets in the 'authorized' room
        io.to('authorized').emit('boilerUpdate', data); 
        console.log(`🖥️ [UI] Broadcasted ${data.boilerId} at ${new Date().toLocaleTimeString()}`);
        
    } catch (e) {
        console.log(`⚠️ [${new Date().toLocaleTimeString()}] Non-JSON: ${message.toString()}`);
    }
});

// THIS IS THE KEY: It opens port 3000
server.listen(3000, () => {
    console.log('🚀 Server started! Open http://localhost:3000 in your browser');
});