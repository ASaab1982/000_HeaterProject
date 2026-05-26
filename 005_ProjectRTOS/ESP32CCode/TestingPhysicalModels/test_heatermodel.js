// test_heatermodel.js
// Standalone simulation of HeaterModel.cpp logic — no Arduino needed.
// Mirrors the exact same math and constants as HeaterModel.cpp.
// Also generates test_heatermodel.html with a Chart.js temperature graph.
//
// Scenario: outdoor = 10°C, heater ON first 60s of every hour, start temp = 10°C

const fs = require('fs');

// --- Constants (must match HeaterModel.cpp) ---
const OUTDOOR_TEMP  = 10.0;   // °C — fixed outdoor temperature for this test
const START_TEMP    = 10.0;   // °C — starting boiler water temperature
const BOILER_MAX    = 90.0;   // °C — physical maximum
const HEAT_RATE     = 1 / 20; // °C/s when heater is ON (1°C per 20s)

const OUTDOOR_COLD  = -5.0;   // °C — lower interpolation bound
const OUTDOOR_WARM  = 30.0;   // °C — upper interpolation bound
const RATE_AT_COLD  = 1 / 600;  // °C/s at -5°C outdoor (1°C per 10 min)
const RATE_AT_WARM  = 1 / 1200; // °C/s at 30°C outdoor (1°C per 20 min)

// --- Cooling rate interpolation (same formula as HeaterModel.cpp) ---
function getCoolingRate(outdoorTemp) {
    const clamped = Math.max(OUTDOOR_COLD, Math.min(OUTDOOR_WARM, outdoorTemp));
    const t = (clamped - OUTDOOR_COLD) / (OUTDOOR_WARM - OUTDOOR_COLD);
    return RATE_AT_COLD + t * (RATE_AT_WARM - RATE_AT_COLD);
}

// --- Heater schedule: ON for first 60 seconds of each hour ---
function isHeaterOn(secondOfDay) {
    return (secondOfDay % 3600) < 60;
}

// --- Format seconds as HH:MM ---
function formatTime(s) {
    const h = String(Math.floor(s / 3600)).padStart(2, '0');
    const m = String(Math.floor((s % 3600) / 60)).padStart(2, '0');
    return `${h}:${m}`;
}

// --- Simulation ---
const TOTAL_SECONDS  = 24 * 3600;
const PRINT_INTERVAL = 1800; // console: print every 30 minutes
const GRAPH_INTERVAL = 60;   // graph: one data point per minute (1440 points)

const coolingRate = getCoolingRate(OUTDOOR_TEMP);

console.log('=======================================================');
console.log(' Heater Model — 24h Simulation');
console.log('=======================================================');
console.log(` Outdoor temp   : ${OUTDOOR_TEMP}°C`);
console.log(` Start temp     : ${START_TEMP}°C`);
console.log(` Heating rate   : +${HEAT_RATE.toFixed(4)} °C/s (1°C per 20s)`);
console.log(` Cooling rate   : -${coolingRate.toFixed(5)} °C/s (${(coolingRate * 60).toFixed(4)} °C/min)`);
console.log(` Heater ON      : first 60s of every hour`);
console.log('=======================================================');
console.log(' Time     | State | Boiler Temp');
console.log('----------|-------|------------');

let boilerTemp   = START_TEMP;
let lastHeaterOn = false;

// Data collected for the graph
const labels = [];
const tempData = [];
const heaterBg = []; // background color per point to show heater ON/OFF

for (let s = 0; s <= TOTAL_SECONDS; s++) {
    const heaterOn = isHeaterOn(s);

    // Console: print on state change
    if (heaterOn !== lastHeaterOn) {
        const marker = heaterOn ? '▶ ON ' : '■ OFF';
        console.log(` ${formatTime(s)}    | ${marker} | ${boilerTemp.toFixed(2)}°C`);
        lastHeaterOn = heaterOn;
    }

    // Console: print every 30 minutes
    if (s % PRINT_INTERVAL === 0 && s % 60 !== 0) {
        const state = heaterOn ? 'ON   ' : 'OFF  ';
        console.log(` ${formatTime(s)}    | ${state} | ${boilerTemp.toFixed(2)}°C`);
    }

    // Graph: collect one point per minute
    if (s % GRAPH_INTERVAL === 0) {
        labels.push(formatTime(s));
        tempData.push(parseFloat(boilerTemp.toFixed(2)));
        heaterBg.push(heaterOn ? 'rgba(255,80,80,0.18)' : 'rgba(0,0,0,0)');
    }

    // Apply 1-second step (mirrors HeaterModel.cpp logic)
    if (heaterOn) {
        boilerTemp += HEAT_RATE * 1;
        if (boilerTemp > BOILER_MAX) boilerTemp = BOILER_MAX;
    } else {
        boilerTemp -= coolingRate * 1;
        if (boilerTemp < OUTDOOR_TEMP) boilerTemp = OUTDOOR_TEMP;
    }
}

console.log('=======================================================');
console.log(` Final boiler temp at 24:00 : ${boilerTemp.toFixed(2)}°C`);
console.log('=======================================================');

// --- Generate HTML graph ---
const html = `<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <title>Heater Model — 24h Simulation</title>
  <script src="https://cdn.jsdelivr.net/npm/chart.js"></script>
  <style>
    body { background: #1a1a2e; color: #e0e0e0; font-family: Arial, sans-serif; padding: 30px; }
    h2   { color: #ff9f43; }
    p    { color: #aaa; font-size: 0.9em; }
    .chart-container { background: #16213e; border-radius: 12px; padding: 20px; max-width: 1100px; }
  </style>
</head>
<body>
  <h2>Heater Model — 24h Simulation</h2>
  <p>
    Outdoor: <b>${OUTDOOR_TEMP}°C</b> &nbsp;|&nbsp;
    Start temp: <b>${START_TEMP}°C</b> &nbsp;|&nbsp;
    Heater ON: <b>first 60s of every hour</b> &nbsp;|&nbsp;
    Cooling rate: <b>${(coolingRate * 60).toFixed(4)} °C/min</b>
  </p>
  <div class="chart-container">
    <canvas id="chart"></canvas>
  </div>
  <script>
    const labels   = ${JSON.stringify(labels)};
    const tempData = ${JSON.stringify(tempData)};
    const heaterBg = ${JSON.stringify(heaterBg)};

    new Chart(document.getElementById('chart'), {
      type: 'line',
      data: {
        labels,
        datasets: [
          {
            label: 'Boiler Water Temp (°C)',
            data: tempData,
            borderColor: '#ff9f43',
            backgroundColor: 'rgba(255,159,67,0.08)',
            borderWidth: 2,
            pointRadius: 0,
            tension: 0.3,
            fill: true,
            yAxisID: 'y'
          },
          {
            label: 'Outdoor Temp (°C)',
            data: new Array(labels.length).fill(${OUTDOOR_TEMP}),
            borderColor: '#54a0ff',
            borderWidth: 1.5,
            borderDash: [6, 4],
            pointRadius: 0,
            fill: false,
            yAxisID: 'y'
          }
        ]
      },
      options: {
        responsive: true,
        interaction: { mode: 'index', intersect: false },
        plugins: {
          legend: { labels: { color: '#e0e0e0' } },
          tooltip: { callbacks: {
            label: ctx => ' ' + ctx.dataset.label + ': ' + ctx.parsed.y.toFixed(2) + '°C'
          }}
        },
        scales: {
          x: {
            ticks: {
              color: '#aaa',
              maxTicksLimit: 25,
              maxRotation: 0
            },
            grid: { color: 'rgba(255,255,255,0.05)' }
          },
          y: {
            ticks: { color: '#aaa', callback: v => v + '°C' },
            grid: { color: 'rgba(255,255,255,0.07)' },
            title: { display: true, text: 'Temperature (°C)', color: '#aaa' }
          }
        }
      }
    });
  </script>
</body>
</html>`;

fs.writeFileSync('test_heatermodel.html', html);
console.log('\n📊 Graph saved → open test_heatermodel.html in your browser');
