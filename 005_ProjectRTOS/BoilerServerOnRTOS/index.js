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

client.on('connect', () => {
    console.log('✅ Connected to HiveMQ');
    client.subscribe('boilers/B1/status');
});

client.on('message', (topic, message) => {
    try {
        const data = JSON.parse(message.toString());
        
        // --- Get the exact time ---
        const now = new Date();
        const timestamp = now.toLocaleTimeString() + `.${now.getMilliseconds()}`;

        // --- 1. DATA RECEIVED FROM HIVE ---
        console.log(`\n🐝 [HIVE] @ ${timestamp}`);
        console.log(`| Boiler: ${data.id} | Temp: ${data.temp}°C | Status: ${data.state}`);
        console.log('---------------------------------------------------------');

        // --- 2. DATA PUSHED TO UI ---
        io.emit('boilerUpdate', data); 
        console.log(`🖥️ [UI] Broadcasted at ${new Date().toLocaleTimeString()}`);
        
    } catch (e) {
        console.log(`⚠️ [${new Date().toLocaleTimeString()}] Non-JSON: ${message.toString()}`);
    }
});

// THIS IS THE KEY: It opens port 3000
server.listen(3000, () => {
    console.log('🚀 Server started! Open http://localhost:3000 in your browser');
});