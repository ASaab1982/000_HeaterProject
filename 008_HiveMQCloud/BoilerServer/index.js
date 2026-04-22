const mqtt = require('mqtt');

// --- Configuration ---
// Note the 'mqtts://' for secure connection on port 8883
const brokerUrl = 'mqtts://d72dc8b632b04c8c91c4702a5b164d59.s1.eu.hivemq.cloud:8883';

const options = {
    clientId: 'Boiler_Manager_Server_' + Math.random().toString(16).substring(2, 8),
    username: 'AhmadSaab',
    password: 'Arda1669*', // Put the password you used in secrets.h here
    clean: true
};

const topic = 'boilers/B1/status';

// --- Connection ---
console.log('Connecting to HiveMQ Cloud...');
const client = mqtt.connect(brokerUrl, options);

client.on('connect', () => {
    console.log('✅ Connected! Watching for boiler data...');
    client.subscribe(topic, (err) => {
        if (!err) {
            console.log(`📡 Subscribed to: ${topic}`);
        }
    });
});

// --- Data Handling ---
client.on('message', (topic, message) => {
    try {
        const data = JSON.parse(message.toString());
        console.log('\n--- DATA RECEIVED ---');
        console.log(`Boiler ID: ${data.id}`);
        console.log(`Temp:      ${data.temp}°C`);
        console.log(`Status:    ${data.state}`);
        console.log('----------------------');
    } catch (e) {
        // In case the message isn't JSON
        console.log(`Message on ${topic}: ${message.toString()}`);
    }
});

client.on('error', (err) => {
    console.log('❌ Connection Error:', err);
});