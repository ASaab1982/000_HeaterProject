// test_hometempmodel.js
// Standalone simulation of HomeTempModel.cpp logic — no Arduino needed.
// Mirrors the exact same math and constants as HomeTempModel.cpp.
// Also generates test_hometempmodel.html with a Chart.js temperature graph.
//
// Phase A (0–8h):   home=10°C, outdoor=25°C, heater OFF, pump OFF, valve 0°
// Phase B (8–16h):  boiler fixed at 45°C,    heater ON,  pump OFF, valve 0°
// Phase C (16–19h): boiler fixed at 45°C,    heater ON,  pump ON,  valve 180°

const fs = require('fs');

// --- Constants (must match HomeTempModel.cpp) ---
const HEAT_GAIN_RATE = 1.0 / 900.0;    // °C/s per °C difference (boiler → home)
const HEAT_LOSS_RATE = 1.0 / 86400.0;  // °C/s per °C difference (home ↔ outdoor, ~3 days to equilibrate)

// --- Valve gain factor (same formula as HomeTempModel.cpp) ---
function getGainFactor(valvePos) {
    return 0.10 + (valvePos / 180.0) * 0.90;
}

// --- Scenario schedule ---
function getScenario(s) {
    const h = s / 3600;
    if (h < 8)  return { pumpOn: false, valvePos: 0,   boilerTemp: null, label: 'A' };
    if (h < 16) return { pumpOn: false, valvePos: 0,   boilerTemp: 45.0, label: 'B' };
                return { pumpOn: true,  valvePos: 180, boilerTemp: 45.0, label: 'C' };
}

function formatTime(s) {
    const h = String(Math.floor(s / 3600)).padStart(2, '0');
    const m = String(Math.floor((s % 3600) / 60)).padStart(2, '0');
    return `${h}:${m}`;
}

// --- Simulation parameters ---
const OUTDOOR_TEMP    = 25.0;  // °C
const START_HOME_TEMP = 10.0;  // °C

const TOTAL_SECONDS  = 19 * 3600;
const PRINT_INTERVAL = 3600;   // console: every hour
const GRAPH_INTERVAL = 60;     // graph: every minute

console.log('=======================================================');
console.log(' Home Temp Model — 19h Simulation');
console.log('=======================================================');
console.log(` Outdoor temp     : ${OUTDOOR_TEMP}°C`);
console.log(` Start home temp  : ${START_HOME_TEMP}°C`);
console.log(` Heat gain rate   : ${(HEAT_GAIN_RATE * 60).toFixed(6)} °C/min per °C (scaled by valve)`);
console.log(` Heat loss rate   : ${(HEAT_LOSS_RATE * 60).toFixed(6)} °C/min per °C (home↔outdoor)`);
console.log(' Phase A (0–8h)  : heater OFF, pump OFF, valve 0°');
console.log(' Phase B (8–16h) : heater ON (boiler 45°C fixed), pump OFF, valve 0°');
console.log(' Phase C (16–19h): heater ON (boiler 45°C fixed), pump ON, valve 180°');
console.log('=======================================================');
console.log(' Time     | Phase | Boiler   | Home    ');
console.log('----------|-------|----------|--------');

let homeTemp = START_HOME_TEMP;

const labels      = [];
const homeData    = [];
const boilerData  = [];
const outdoorLine = [];
const phaseBg     = [];

for (let s = 0; s <= TOTAL_SECONDS; s++) {
    const { pumpOn, valvePos, boilerTemp, label } = getScenario(s);
    const gainFactor = getGainFactor(valvePos);

    // Console: print every hour
    if (s % PRINT_INTERVAL === 0) {
        const boilerStr = boilerTemp !== null ? boilerTemp.toFixed(1) + '°C' : '  OFF ';
        console.log(` ${formatTime(s)}     |  ${label}    | ${boilerStr.padStart(7)}  | ${homeTemp.toFixed(2)}°C`);
    }

    // Graph: collect one point per minute
    if (s % GRAPH_INTERVAL === 0) {
        labels.push(formatTime(s));
        homeData.push(parseFloat(homeTemp.toFixed(3)));
        boilerData.push(boilerTemp !== null ? boilerTemp : null);
        outdoorLine.push(OUTDOOR_TEMP);
        if (label === 'A') phaseBg.push('rgba(84,160,255,0.10)');
        else if (label === 'B') phaseBg.push('rgba(255,159,67,0.10)');
        else                    phaseBg.push('rgba(100,220,120,0.15)');
    }

    // --- Home model (1s step, mirrors HomeTempModel.cpp) ---
    if (pumpOn && boilerTemp !== null) {
        const gain = HEAT_GAIN_RATE * gainFactor * (boilerTemp - homeTemp);
        if (gain > 0) homeTemp += gain;
    }
    // Heat exchange with outdoors (symmetric — no floor clamp)
    const exchange = HEAT_LOSS_RATE * (homeTemp - OUTDOOR_TEMP);
    homeTemp -= exchange;
}

console.log('=======================================================');
console.log(` Final home temp at 19:00 : ${homeTemp.toFixed(2)}°C`);
console.log('=======================================================');

// --- Generate HTML graph ---
const html = `<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <title>Home Temp Model — 19h Simulation</title>
  <script src="https://cdn.jsdelivr.net/npm/chart.js"></script>
  <style>
    body { background: #1a1a2e; color: #e0e0e0; font-family: Arial, sans-serif; padding: 30px; }
    h2   { color: #54a0ff; }
    p    { color: #aaa; font-size: 0.9em; }
    .legend-phases { font-size: 0.85em; margin-bottom: 15px; }
    .legend-phases span { display: inline-block; padding: 3px 12px; border-radius: 4px; margin-right: 10px; }
    .phase-a { background: rgba(84,160,255,0.20); }
    .phase-b { background: rgba(255,159,67,0.20); }
    .phase-c { background: rgba(100,220,120,0.25); }
    .chart-container { background: #16213e; border-radius: 12px; padding: 20px; max-width: 1100px; }
  </style>
</head>
<body>
  <h2>Home Temp Model — 19h Simulation</h2>
  <p>
    Outdoor: <b>${OUTDOOR_TEMP}°C</b> &nbsp;|&nbsp;
    Start home: <b>${START_HOME_TEMP}°C</b> &nbsp;|&nbsp;
    Loss rate: <b>${(HEAT_LOSS_RATE * 60).toFixed(6)} °C/min/°C</b> &nbsp;|&nbsp;
    Gain rate (180°): <b>${(HEAT_GAIN_RATE * getGainFactor(180) * 60).toFixed(6)} °C/min/°C</b>
  </p>
  <div class="legend-phases">
    <span class="phase-a">Phase A (0–8h): No heating, no pump</span>
    <span class="phase-b">Phase B (8–16h): Boiler 45°C, pump OFF</span>
    <span class="phase-c">Phase C (16–19h): Boiler 45°C, pump ON, valve 180°</span>
  </div>
  <div class="chart-container">
    <canvas id="chart"></canvas>
  </div>
  <script>
    const labels      = ${JSON.stringify(labels)};
    const homeData    = ${JSON.stringify(homeData)};
    const boilerData  = ${JSON.stringify(boilerData)};
    const outdoorLine = ${JSON.stringify(outdoorLine)};
    const phaseBg     = ${JSON.stringify(phaseBg)};

    new Chart(document.getElementById('chart'), {
      type: 'line',
      data: {
        labels,
        datasets: [
          {
            label: 'Home Temp (°C)',
            data: homeData,
            borderColor: '#54a0ff',
            backgroundColor: 'rgba(84,160,255,0.08)',
            borderWidth: 2.5,
            pointRadius: 0,
            tension: 0.3,
            fill: true,
            yAxisID: 'y'
          },
          {
            label: 'Boiler Water Temp (°C)',
            data: boilerData,
            borderColor: '#ff9f43',
            borderWidth: 1.5,
            borderDash: [4, 3],
            pointRadius: 0,
            tension: 0,
            fill: false,
            yAxisID: 'y',
            spanGaps: false
          },
          {
            label: 'Outdoor Temp (°C)',
            data: outdoorLine,
            borderColor: '#aaa',
            borderWidth: 1,
            borderDash: [6, 4],
            pointRadius: 0,
            fill: false,
            yAxisID: 'y'
          },
          {
            label: 'Phase',
            data: phaseBg.map(() => null),
            backgroundColor: phaseBg,
            type: 'bar',
            barPercentage: 1.3,
            categoryPercentage: 1.0,
            yAxisID: 'y',
            order: 10
          }
        ]
      },
      options: {
        responsive: true,
        interaction: { mode: 'index', intersect: false },
        plugins: {
          legend: {
            labels: {
              color: '#e0e0e0',
              filter: item => item.text !== 'Phase'
            }
          },
          tooltip: {
            filter: item => item.dataset.label !== 'Phase',
            callbacks: {
              label: ctx => ' ' + ctx.dataset.label + ': ' + (ctx.parsed.y != null ? ctx.parsed.y.toFixed(2) + '°C' : 'OFF')
            }
          }
        },
        scales: {
          x: {
            ticks: { color: '#aaa', maxTicksLimit: 20, maxRotation: 0 },
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

fs.writeFileSync('test_hometempmodel.html', html);
console.log('\n📊 Graph saved → open test_hometempmodel.html in your browser');
