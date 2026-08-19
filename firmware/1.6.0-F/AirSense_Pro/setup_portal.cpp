#include "setup_portal.h"
#include "setup_html.h"
#include "storage.h"
#include "wifi_manager.h"
#include "globals.h"
#include "air_quality.h"
#include "version.h"
#include "api.h"

#include <WiFi.h>
#include <WebServer.h>

WebServer server(80);

static bool portalRunning = false;


// =====================================================
// Forward Declarations
// =====================================================

void handleRoot();
void handleWiFiScan();
void handleWiFiConnect();
void handleDashboard();
// =====================================================
// Root Page
// =====================================================

void handleRoot()
{
    server.send(
        200,
        "text/html",
        SETUP_HTML
    );
}
//=====================================================
//Dashboard
void handleDashboard()
{
    String page = R"rawliteral(
<!DOCTYPE html>
<html>

<head>

<meta charset="UTF-8">

<meta name="viewport"
      content="width=device-width, initial-scale=1.0">

<title>AirSense Pro</title>

<style>
:root{--bg:#070b12;--panel:#0d141f;--panel2:#111b28;--line:#1d2a3a;--text:#edf5ff;--muted:#7f91a8;--accent:#43b8ff;--accent2:#7c6cff;--good:#36d98b;--warn:#ffc857;--bad:#ff6577;--shadow:0 18px 50px rgba(0,0,0,.28)}
*{box-sizing:border-box}html{scroll-behavior:smooth}body{margin:0;padding:24px;font-family:Inter,ui-sans-serif,system-ui,-apple-system,Segoe UI,Roboto,Arial,sans-serif;background:radial-gradient(circle at 15% 0%,rgba(67,184,255,.10),transparent 28%),radial-gradient(circle at 90% 10%,rgba(124,108,255,.10),transparent 25%),var(--bg);color:var(--text)}
.container{max-width:1280px;margin:auto}.header{display:flex;align-items:center;justify-content:space-between;gap:18px;margin:0 0 22px;padding:18px 20px;border:1px solid var(--line);border-radius:20px;background:rgba(13,20,31,.82);box-shadow:var(--shadow);backdrop-filter:blur(14px)}.logo{font-size:28px;font-weight:800;letter-spacing:-.6px}.logo:before{content:'◉';color:var(--accent);font-size:18px;margin-right:9px;text-shadow:0 0 16px rgba(67,184,255,.8)}.subtitle{color:var(--muted);font-size:12px;margin-top:4px}.header:after{content:'v1.6.0-E';color:var(--muted);font:600 11px ui-monospace,SFMono-Regular,Consolas,monospace;padding:7px 10px;border:1px solid var(--line);border-radius:999px}
.top-row{display:grid;grid-template-columns:1.35fr .85fr;gap:18px;margin-bottom:18px}.dashboard-grid{display:grid;grid-template-columns:repeat(2,minmax(0,1fr));gap:18px;margin-bottom:18px}.card,.graph-card{margin:0;background:linear-gradient(145deg,rgba(17,27,40,.96),rgba(10,16,25,.96));border:1px solid var(--line);border-radius:20px;padding:20px;box-shadow:var(--shadow);min-width:0}.section-title{display:flex;align-items:center;gap:9px;font-size:13px;font-weight:800;letter-spacing:.9px;text-transform:uppercase;color:#c8d7e8;margin-bottom:16px}.section-title:before{content:'';width:7px;height:7px;border-radius:50%;background:var(--accent);box-shadow:0 0 12px rgba(67,184,255,.8)}
.row{display:flex;justify-content:space-between;align-items:center;gap:16px;padding:13px 0;border-bottom:1px solid rgba(255,255,255,.055)}.row:last-child{border-bottom:0}.label{color:var(--muted);font-size:13px}.value{font-size:22px;font-weight:800;color:var(--text);letter-spacing:-.4px}.status{font-weight:800;font-size:13px}.connected{color:var(--good)}.disconnected{color:var(--bad)}
.air-quality-box{margin-top:16px;padding:18px;border-radius:17px;background:radial-gradient(circle at 50% 0%,rgba(67,184,255,.13),transparent 55%),#0a111b;border:1px solid var(--line);text-align:center}.aq-ring{width:112px;height:112px;margin:2px auto 10px;border-radius:50%;background:conic-gradient(var(--accent) calc(var(--aq-pct,45)*1%),#162333 0);display:grid;place-items:center;box-shadow:0 0 24px rgba(67,184,255,.10)}.aq-ring:after{content:"";width:88px;height:88px;border-radius:50%;background:#0a111b;border:1px solid var(--line);grid-area:1/1}.aq-ring-value{grid-area:1/1;position:relative;z-index:1;font-size:23px;font-weight:900}.air-quality-title{font-size:10px;font-weight:800;letter-spacing:1.5px;color:var(--muted)}.air-quality-score{font-size:42px;font-weight:900;line-height:1.1;margin-top:6px}.air-quality-status{margin-top:5px;font-size:14px;font-weight:900;letter-spacing:.5px}.air-quality-message{margin-top:5px;font-size:11px;color:var(--muted)}.aq-good{color:var(--good)}.aq-moderate{color:var(--warn)}.aq-poor,.aq-unhealthy{color:var(--bad)}
.health{margin-top:14px;padding:11px;border-radius:12px;text-align:center;font-weight:800;font-size:12px;background:rgba(54,217,139,.08);border:1px solid rgba(54,217,139,.2);color:var(--good)}.unhealthy{background:rgba(255,101,119,.08);border-color:rgba(255,101,119,.2);color:var(--bad)}.updated,.diagnostic-footer,.graph-info{color:var(--muted);font-size:10px;text-align:center;margin-top:9px}.error{color:var(--bad);text-align:center;font-size:12px;display:none;margin-top:10px}
.diagnostic-grid{display:grid;grid-template-columns:repeat(3,minmax(0,1fr));gap:9px}.metric,.history-stat{background:rgba(255,255,255,.025);border:1px solid var(--line);border-radius:13px;padding:11px;min-width:0}.metric span,.history-stat-title{display:block;color:var(--muted);font-size:10px;margin-bottom:5px}.metric strong{display:block;color:var(--text);font-size:15px;overflow:hidden;text-overflow:ellipsis;white-space:nowrap}.graph-container{width:100%;height:250px;position:relative;overflow:hidden}.graph-container canvas{width:100%;height:100%;display:block}.graph-card{margin-bottom:0}.graph-card .section-title{margin-bottom:12px}
button{border:1px solid var(--line);border-radius:10px;padding:8px 12px;background:#111d2b;color:#cfe4f8;cursor:pointer;font-weight:800;font-size:11px;transition:.18s}.history-controls{display:flex;gap:7px;flex-wrap:wrap;align-items:center}.history-controls button:hover{border-color:var(--accent);color:white;transform:translateY(-1px)}.history-controls button:active{transform:translateY(0)}#historyStatus{color:var(--muted)!important;font-size:10px!important;margin-left:auto}.history-stats{display:grid;grid-template-columns:repeat(5,minmax(0,1fr));gap:9px;margin-top:14px}.history-stat{text-align:center}.history-stat-value{font-size:12px;font-weight:800;color:var(--accent)}.footer{text-align:center;color:var(--muted);font-size:10px;margin-top:24px;padding-bottom:8px}.footer:before{content:'AIR QUALITY • SENSOR TELEMETRY • LOCAL CONTROL';display:block;letter-spacing:1.4px;margin-bottom:6px;color:#51647b}
@media(max-width:900px){.top-row{grid-template-columns:1fr}.dashboard-grid{grid-template-columns:1fr}.history-stats{grid-template-columns:repeat(2,minmax(0,1fr))}}@media(max-width:700px){body{padding:12px}.header{padding:15px}.logo{font-size:23px}.diagnostic-grid{grid-template-columns:repeat(2,minmax(0,1fr))}.graph-container{height:220px}#historyStatus{width:100%;margin-left:0}.value{font-size:19px}}
</style>

</head>


<body>
<div class="container">
  <div class="header">
    <div><div class="logo">AirSense Pro</div><div class="subtitle">Smart Environmental Intelligence • Local IoT Monitor</div></div>
  </div>

  <div class="top-row">
    <div class="card">
      <div class="section-title">Live Environment</div>
      <div class="row"><span class="label">Temperature</span><span class="value" id="temperature">-- °C</span></div>
      <div class="row"><span class="label">Humidity</span><span class="value" id="humidity">-- %</span></div>
      <div class="row"><span class="label">Pressure</span><span class="value" id="pressure">-- hPa</span></div>
      <div class="row"><span class="label">Gas Resistance</span><span class="value" id="gasResistance">-- kΩ</span></div>
      <div class="updated" id="sensorUpdated">Waiting for sensor data...</div>
      <div class="air-quality-box"><div class="air-quality-title">AIR QUALITY SCORE</div><div class="aq-ring" id="aqRing"><div class="aq-ring-value" id="aqRingValue">--</div></div><div class="air-quality-score" id="airQualityScore">-- / 100</div><div class="air-quality-status" id="airQualityStatus">Checking...</div><div class="air-quality-message" id="airQualityMessage">Waiting for air quality data...</div><div class="updated" id="aqDetails">Relative index • baseline -- kΩ</div></div>
      <div class="error" id="sensorError">Unable to read sensor data</div>
    </div>

    <div class="card">
      <div class="section-title">Device Status</div>
      <div class="row"><span class="label">Wi-Fi</span><span class="status" id="wifiStatus">Checking...</span></div>
      <div class="row"><span class="label">SSID</span><span id="ssid">--</span></div>
      <div class="row"><span class="label">IP Address</span><span id="ipAddress">--</span></div>
      <div class="row"><span class="label">Signal</span><span id="rssi">-- dBm</span></div>
      <div class="row"><span class="label">Firmware</span><span id="firmware">--</span></div>
      <div class="row"><span class="label">Uptime</span><span id="uptime">--</span></div>
      <div class="health" id="health">Checking system...</div>
      <div class="updated" id="systemUpdated">Waiting for system data...</div>
      <div class="error" id="systemError">Unable to read system information</div>
    </div>
  </div>

  <div class="card diagnostics-card" style="margin-bottom:18px">
    <div class="section-title">System Telemetry</div>
    <div class="diagnostic-grid">
      <div class="metric"><span>Free Heap</span><strong id="freeHeap">--</strong></div>
      <div class="metric"><span>Sensor Reads</span><strong id="sensorReads">--</strong></div><div class="metric"><span>AQ Baseline</span><strong id="aqBaseline">--</strong></div>
      <div class="metric"><span>Sensor Failures</span><strong id="sensorFailures">--</strong></div>
      <div class="metric"><span>Recoveries</span><strong id="sensorRecoveries">--</strong></div>
      <div class="metric"><span>Offline Events</span><strong id="sensorOfflineEvents">--</strong></div>
      <div class="metric"><span>Wi-Fi Drops</span><strong id="wifiDisconnects">--</strong></div>
      <div class="metric"><span>Wi-Fi Reconnects</span><strong id="wifiReconnects">--</strong></div>
      <div class="metric"><span>TCP Connections</span><strong id="tcpConnections">--</strong></div>
      <div class="metric"><span>TCP Disconnects</span><strong id="tcpDisconnects">--</strong></div>
      <div class="metric"><span>History</span><strong id="historySamples">--</strong></div>
      <div class="metric"><span>History Capacity</span><strong id="historyCapacity">--</strong></div>
      <div class="metric"><span>History Interval</span><strong id="historyInterval">--</strong></div>
    </div>
    <div class="diagnostic-footer" id="diagnosticFooter">Waiting for diagnostics...</div>
  </div>

  <div class="graph-card" style="margin-bottom:18px">
    <div class="section-title">Historical Telemetry</div>
    <div class="history-controls">
      <button onclick="setHistoryWindow('1h')">1H</button><button onclick="setHistoryWindow('6h')">6H</button><button onclick="setHistoryWindow('12h')">12H</button><button onclick="setHistoryWindow('24h')">24H</button>
      <span id="historyStatus">Loading history...</span>
    </div>
    <div class="history-stats">
      <div class="history-stat"><div class="history-stat-title">Temperature</div><div class="history-stat-value" id="statTemperature">--</div></div>
      <div class="history-stat"><div class="history-stat-title">Humidity</div><div class="history-stat-value" id="statHumidity">--</div></div>
      <div class="history-stat"><div class="history-stat-title">Pressure</div><div class="history-stat-value" id="statPressure">--</div></div>
      <div class="history-stat"><div class="history-stat-title">Gas Resistance</div><div class="history-stat-value" id="statGas">--</div></div>
      <div class="history-stat"><div class="history-stat-title">Air Quality</div><div class="history-stat-value" id="statAirQuality">--</div></div>
    </div>
  </div>

  <div class="dashboard-grid">
    <div class="graph-card"><div class="section-title">Temperature History</div><div class="graph-container"><canvas id="temperatureGraph"></canvas></div><div class="graph-info" id="temperatureGraphInfo">Collecting temperature data...</div></div>
    <div class="graph-card"><div class="section-title">Humidity History</div><div class="graph-container"><canvas id="humidityGraph"></canvas></div><div class="graph-info" id="humidityGraphInfo">Collecting humidity data...</div></div>
    <div class="graph-card"><div class="section-title">Pressure History</div><div class="graph-container"><canvas id="pressureGraph"></canvas></div><div class="graph-info" id="pressureGraphInfo">Collecting pressure data...</div></div>
    <div class="graph-card"><div class="section-title">Gas Resistance History</div><div class="graph-container"><canvas id="gasResistanceGraph"></canvas></div><div class="graph-info" id="gasResistanceGraphInfo">Collecting gas resistance data...</div></div>
    <div class="graph-card" style="grid-column:1/-1"><div class="section-title">Air Quality Score History</div><div class="graph-container"><canvas id="airQualityGraph"></canvas></div><div class="graph-info" id="airQualityGraphInfo">Collecting air quality history...</div></div>
  </div>

  <div class="footer">AirSense Pro • Local Environmental Intelligence</div>
</div>

<script>


// =====================================================
// Format Uptime
// =====================================================

function formatUptime(seconds)
{
    let days = Math.floor(seconds / 86400);

    seconds %= 86400;

    let hours = Math.floor(seconds / 3600);

    seconds %= 3600;

    let minutes = Math.floor(seconds / 60);

    seconds %= 60;


    let result = "";

    if (days > 0)
    {
        result += days + "d ";
    }

    result += String(hours).padStart(2, "0");
    result += ":";
    result += String(minutes).padStart(2, "0");
    result += ":";
    result += String(seconds).padStart(2, "0");


    return result;
}


// =====================================================
// Temperature History Graph
// =====================================================

const temperatureHistory = [];
const humidityHistory = [];
const pressureHistory = [];
const gasResistanceHistory = [];
const airQualityHistory = [];

const TEMPERATURE_HISTORY_LIMIT = 288;
const HUMIDITY_HISTORY_LIMIT = 288;
const PRESSURE_HISTORY_LIMIT = 288;
const GAS_RESISTANCE_HISTORY_LIMIT = 288;
const AIR_QUALITY_HISTORY_LIMIT = 288;

let selectedHistoryWindow = "6h";


function addTemperatureHistory(value)
{
    if (!Number.isFinite(value))
    {
        return;
    }

    temperatureHistory.push({
        value: value,
        time: new Date()
    });

    if (temperatureHistory.length >
        TEMPERATURE_HISTORY_LIMIT)
    {
        temperatureHistory.shift();
    }

    drawTemperatureGraph();
}


function drawTemperatureGraph()
{
    const canvas =
        document.getElementById(
            "temperatureGraph"
        );

    if (!canvas)
    {
        return;
    }

    const container =
        canvas.parentElement;

    const width =
        container.clientWidth;

    const height =
        container.clientHeight;

    const dpr =
        window.devicePixelRatio || 1;

    canvas.width =
        width * dpr;

    canvas.height =
        height * dpr;

    canvas.style.width =
        width + "px";

    canvas.style.height =
        height + "px";

    const ctx =
        canvas.getContext("2d");

    ctx.setTransform(
        dpr,
        0,
        0,
        dpr,
        0,
        0
    );

    ctx.clearRect(
        0,
        0,
        width,
        height
    );

    if (temperatureHistory.length === 0)
    {
        ctx.fillStyle = "#888";
        ctx.font = "13px Arial";
        ctx.textAlign = "center";

        ctx.fillText(
            "Waiting for temperature data...",
            width / 2,
            height / 2
        );

        return;
    }

    const left = 48;
    const right = 12;
    const top = 15;
    const bottom = 28;

    const graphWidth =
        width - left - right;

    const graphHeight =
        height - top - bottom;

    let minValue =
        Math.min(
            ...temperatureHistory.map(
                point => point.value
            )
        );

    let maxValue =
        Math.max(
            ...temperatureHistory.map(
                point => point.value
            )
        );

    if (maxValue - minValue < 2)
    {
        const center =
            (maxValue + minValue) / 2;

        minValue = center - 1;
        maxValue = center + 1;
    }

    const padding =
        (maxValue - minValue) * 0.10;

    minValue -= padding;
    maxValue += padding;

    ctx.strokeStyle = "#e5e5e5";
    ctx.lineWidth = 1;

    ctx.fillStyle = "#777";
    ctx.font = "11px Arial";
    ctx.textAlign = "right";

    const gridLines = 4;

    for (let i = 0; i <= gridLines; i++)
    {
        const y =
            top +
            (graphHeight * i / gridLines);

        ctx.beginPath();

        ctx.moveTo(left, y);
        ctx.lineTo(width - right, y);

        ctx.stroke();

        const value =
            maxValue -
            ((maxValue - minValue) *
             i / gridLines);

        ctx.fillText(
            value.toFixed(1) + "°",
            left - 6,
            y + 4
        );
    }

    ctx.strokeStyle = "#1976d2";
    ctx.lineWidth = 2.5;
    ctx.lineJoin = "round";
    ctx.lineCap = "round";

    ctx.beginPath();

    temperatureHistory.forEach(
        (point, index) =>
        {
            const x =
                temperatureHistory.length === 1
                ? left + graphWidth / 2
                : left +
                  (graphWidth *
                   index /
                   (temperatureHistory.length - 1));

            const y =
                top +
                graphHeight *
                (maxValue - point.value) /
                (maxValue - minValue);

            if (index === 0)
            {
                ctx.moveTo(x, y);
            }
            else
            {
                ctx.lineTo(x, y);
            }
        }
    );

    ctx.stroke();

    const latest =
        temperatureHistory[
            temperatureHistory.length - 1
        ];

    const latestX =
        temperatureHistory.length === 1
        ? left + graphWidth / 2
        : left + graphWidth;

    const latestY =
        top +
        graphHeight *
        (maxValue - latest.value) /
        (maxValue - minValue);

    ctx.fillStyle = "#1976d2";

    ctx.beginPath();

    ctx.arc(
        latestX,
        latestY,
        4,
        0,
        Math.PI * 2
    );

    ctx.fill();

    ctx.fillStyle = "#888";
    ctx.font = "10px Arial";
    ctx.textAlign = "center";

    const first =
        temperatureHistory[0];

    if (first)
    {
        ctx.fillText(
            first.time.toLocaleTimeString(),
            left,
            height - 8
        );
    }

    if (temperatureHistory.length > 1)
    {
        ctx.fillText(
            latest.time.toLocaleTimeString(),
            width - right,
            height - 8
        );
    }

    const info =
        document.getElementById(
            "temperatureGraphInfo"
        );

    if (info)
    {
        info.textContent =
            temperatureHistory.length +
            " / " +
            TEMPERATURE_HISTORY_LIMIT +
            " readings • Last: " +
            latest.value.toFixed(2) +
            " °C";
    }
}



// =====================================================
// Reusable Graph Renderer
// =====================================================

function drawHistoryGraph(
    canvasId,
    infoId,
    history,
    limit,
    unit,
    emptyText
)
{
    const canvas =
        document.getElementById(canvasId);

    if (!canvas)
    {
        return;
    }

    const container =
        canvas.parentElement;

    const width =
        container.clientWidth;

    const height =
        container.clientHeight;

    const dpr =
        window.devicePixelRatio || 1;

    canvas.width =
        width * dpr;

    canvas.height =
        height * dpr;

    canvas.style.width =
        width + "px";

    canvas.style.height =
        height + "px";

    const ctx =
        canvas.getContext("2d");

    ctx.setTransform(
        dpr,
        0,
        0,
        dpr,
        0,
        0
    );

    ctx.clearRect(
        0,
        0,
        width,
        height
    );

    if (history.length === 0)
    {
        ctx.fillStyle = "#888";
        ctx.font = "13px Arial";
        ctx.textAlign = "center";

        ctx.fillText(
            emptyText,
            width / 2,
            height / 2
        );

        return;
    }

    // Extra left margin keeps 4-digit pressure labels
    // fully visible while preserving the temperature graph layout.
    // Standardized plot geometry for all reusable history graphs.
    // Keep identical margins so Temperature, Humidity and Pressure
    // share the same plot area and axis alignment.
    const left = 72;
    const right = 16;
    const top = 18;
    const bottom = 30;

    const graphWidth =
        width - left - right;

    const graphHeight =
        height - top - bottom;

    let minValue =
        Math.min(
            ...history.map(
                point => point.value
            )
        );

    let maxValue =
        Math.max(
            ...history.map(
                point => point.value
            )
        );

    if (maxValue - minValue < 2)
    {
        const center =
            (maxValue + minValue) / 2;

        minValue =
            center - 1;

        maxValue =
            center + 1;
    }

    const padding =
        (maxValue - minValue) * 0.10;

    minValue -= padding;
    maxValue += padding;

    ctx.strokeStyle = "#e5e5e5";
    ctx.lineWidth = 1;

    ctx.fillStyle = "#777";
    ctx.font = "11px Arial";
    ctx.textAlign = "right";

    const gridLines = 4;

    for (let i = 0; i <= gridLines; i++)
    {
        const y =
            top +
            (graphHeight * i / gridLines);

        ctx.beginPath();

        ctx.moveTo(left, y);
        ctx.lineTo(width - right, y);

        ctx.stroke();

        const value =
            maxValue -
            ((maxValue - minValue) *
             i / gridLines);

        ctx.fillText(
            value.toFixed(1) + unit,
            left - 6,
            y + 4
        );
    }

    ctx.strokeStyle = "#1976d2";
    ctx.lineWidth = 2.5;
    ctx.lineJoin = "round";
    ctx.lineCap = "round";

    ctx.beginPath();

    history.forEach(
        (point, index) =>
        {
            const x =
                history.length === 1
                ? left + graphWidth / 2
                : left +
                  (graphWidth *
                   index /
                   (history.length - 1));

            const y =
                top +
                graphHeight *
                (maxValue - point.value) /
                (maxValue - minValue);

            if (index === 0)
            {
                ctx.moveTo(x, y);
            }
            else
            {
                ctx.lineTo(x, y);
            }
        }
    );

    ctx.stroke();

    const latest =
        history[
            history.length - 1
        ];

    const latestX =
        history.length === 1
        ? left + graphWidth / 2
        : left + graphWidth;

    const latestY =
        top +
        graphHeight *
        (maxValue - latest.value) /
        (maxValue - minValue);

    ctx.fillStyle = "#1976d2";

    ctx.beginPath();

    ctx.arc(
        latestX,
        latestY,
        4,
        0,
        Math.PI * 2
    );

    ctx.fill();

    ctx.fillStyle = "#888";
    ctx.font = "10px Arial";
    ctx.textAlign = "center";

    const first =
        history[0];

    if (first)
    {
        ctx.fillText(
            first.time.toLocaleTimeString(),
            left,
            height - 8
        );
    }

    if (history.length > 1)
    {
        ctx.fillText(
            latest.time.toLocaleTimeString(),
            width - right,
            height - 8
        );
    }

    const info =
        document.getElementById(infoId);

    if (info)
    {
        info.textContent =
            history.length +
            " / " +
            limit +
            " readings • Last: " +
            latest.value.toFixed(2) +
            " " +
            unit;
    }
}


function addHumidityHistory(value)
{
    if (!Number.isFinite(value))
    {
        return;
    }

    humidityHistory.push({
        value: value,
        time: new Date()
    });

    if (humidityHistory.length >
        HUMIDITY_HISTORY_LIMIT)
    {
        humidityHistory.shift();
    }

    drawHistoryGraph(
        "humidityGraph",
        "humidityGraphInfo",
        humidityHistory,
        HUMIDITY_HISTORY_LIMIT,
        "%",
        "Waiting for humidity data..."
    );
}


function addPressureHistory(value)
{
    if (!Number.isFinite(value))
    {
        return;
    }

    pressureHistory.push({
        value: value,
        time: new Date()
    });

    if (pressureHistory.length >
        PRESSURE_HISTORY_LIMIT)
    {
        pressureHistory.shift();
    }

    drawHistoryGraph(
        "pressureGraph",
        "pressureGraphInfo",
        pressureHistory,
        PRESSURE_HISTORY_LIMIT,
        " hPa",
        "Waiting for pressure data..."
    );
}


function addGasResistanceHistory(value)
{
    if (!Number.isFinite(value))
    {
        return;
    }

    gasResistanceHistory.push({
        value: value,
        time: new Date()
    });

    if (gasResistanceHistory.length >
        GAS_RESISTANCE_HISTORY_LIMIT)
    {
        gasResistanceHistory.shift();
    }

    drawHistoryGraph(
        "gasResistanceGraph",
        "gasResistanceGraphInfo",
        gasResistanceHistory,
        GAS_RESISTANCE_HISTORY_LIMIT,
        " kΩ",
        "Waiting for gas resistance data..."
    );
}



function addAirQualityHistory(value, time)
{
    if (!Number.isFinite(value))
    {
        return;
    }

    airQualityHistory.push({
        value: value,
        time: time || new Date()
    });

    if (airQualityHistory.length > AIR_QUALITY_HISTORY_LIMIT)
    {
        airQualityHistory.shift();
    }

    drawHistoryGraph(
        "airQualityGraph",
        "airQualityGraphInfo",
        airQualityHistory,
        AIR_QUALITY_HISTORY_LIMIT,
        "",
        "Waiting for air quality history..."
    );
}


function formatStat(stat, unit, decimals)
{
    if (!stat || !Number.isFinite(Number(stat.avg)))
        return "--";

    return "Min " + Number(stat.min).toFixed(decimals) + unit +
           " • Avg " + Number(stat.avg).toFixed(decimals) + unit +
           " • Max " + Number(stat.max).toFixed(decimals) + unit;
}

function updateHistoryStatistics(stats)
{
    stats = stats || {};
    document.getElementById("statTemperature").textContent = formatStat(stats.temperature, "°C", 1);
    document.getElementById("statHumidity").textContent = formatStat(stats.humidity, "%", 1);
    document.getElementById("statPressure").textContent = formatStat(stats.pressure, "", 1) + " hPa";
    document.getElementById("statGas").textContent = formatStat(stats.gasResistance, "", 1) + " kΩ";
    document.getElementById("statAirQuality").textContent = formatStat(stats.airQualityScore, "", 1) + " /100";
}

function setHistoryWindow(windowValue)
{
    selectedHistoryWindow = windowValue;
    loadHistory();
}


function loadHistory()
{
    const status = document.getElementById("historyStatus");

    if (status)
    {
        status.textContent = "Loading " + selectedHistoryWindow + " history...";
    }

    fetch("/api/history?window=" + encodeURIComponent(selectedHistoryWindow))
        .then(response =>
        {
            if (!response.ok)
            {
                throw new Error("History HTTP error");
            }

            return response.json();
        })
        .then(data =>
        {
            temperatureHistory.length = 0;
            humidityHistory.length = 0;
            pressureHistory.length = 0;
            gasResistanceHistory.length = 0;
            airQualityHistory.length = 0;

            const samples = Array.isArray(data.samples) ? data.samples : [];
            const interval = Number(data.sampleIntervalSeconds) || 300;
            updateHistoryStatistics(data.statistics);

            samples.forEach((sample, index) =>
            {
                // The firmware stores uptime rather than wall-clock time.
                // Build a stable relative display timeline from the sample
                // sequence so history remains useful across reboots.
                const ageSeconds = (samples.length - 1 - index) * interval;
                const sampleTime = new Date(Date.now() - ageSeconds * 1000);

                const t = Number(sample.temperature);
                const h = Number(sample.humidity);
                const p = Number(sample.pressure);
                const g = Number(sample.gasResistance);
                const aq = Number(sample.airQualityScore);

                if (Number.isFinite(t))
                    temperatureHistory.push({value:t, time:sampleTime});

                if (Number.isFinite(h))
                    humidityHistory.push({value:h, time:sampleTime});

                if (Number.isFinite(p))
                    pressureHistory.push({value:p, time:sampleTime});

                if (Number.isFinite(g))
                    gasResistanceHistory.push({value:g, time:sampleTime});

                if (Number.isFinite(aq))
                    airQualityHistory.push({value:aq, time:sampleTime});
            });

            drawTemperatureGraph();

            drawHistoryGraph(
                "humidityGraph",
                "humidityGraphInfo",
                humidityHistory,
                AIR_QUALITY_HISTORY_LIMIT,
                "%",
                "Waiting for humidity history..."
            );

            drawHistoryGraph(
                "pressureGraph",
                "pressureGraphInfo",
                pressureHistory,
                PRESSURE_HISTORY_LIMIT,
                " hPa",
                "Waiting for pressure history..."
            );

            drawHistoryGraph(
                "gasResistanceGraph",
                "gasResistanceGraphInfo",
                gasResistanceHistory,
                GAS_RESISTANCE_HISTORY_LIMIT,
                " kΩ",
                "Waiting for gas resistance history..."
            );

            drawHistoryGraph(
                "airQualityGraph",
                "airQualityGraphInfo",
                airQualityHistory,
                AIR_QUALITY_HISTORY_LIMIT,
                "",
                "Waiting for air quality history..."
            );

            if (status)
            {
                status.textContent =
                    samples.length +
                    " valid samples • " +
                    Math.round(interval / 60) +
                    " min interval • " +
                    selectedHistoryWindow;
            }
        })
        .catch(error =>
        {
            console.log("History API Error:", error);

            if (status)
            {
                status.textContent = "History unavailable";
            }
        });
}


window.addEventListener(
    "resize",
    function()
    {
        drawTemperatureGraph();

        drawHistoryGraph(
            "humidityGraph",
            "humidityGraphInfo",
            humidityHistory,
            HUMIDITY_HISTORY_LIMIT,
            "%",
            "Waiting for humidity data..."
        );

        drawHistoryGraph(
            "pressureGraph",
            "pressureGraphInfo",
            pressureHistory,
            PRESSURE_HISTORY_LIMIT,
            " hPa",
            "Waiting for pressure data..."
        );

        drawHistoryGraph(
            "gasResistanceGraph",
            "gasResistanceGraphInfo",
            gasResistanceHistory,
            GAS_RESISTANCE_HISTORY_LIMIT,
            " kΩ",
            "Waiting for gas resistance data..."
        );

        drawHistoryGraph(
            "airQualityGraph",
            "airQualityGraphInfo",
            airQualityHistory,
            AIR_QUALITY_HISTORY_LIMIT,
            "",
            "Waiting for air quality history..."
        );
    }
);


window.addEventListener(
    "resize",
    drawTemperatureGraph
);


// =====================================================
// Sensor Data
// =====================================================

function updateSensorData()
{

    fetch("/api/sensor")

        .then(response =>
        {

            if (!response.ok)
            {
                throw new Error("Sensor HTTP error");
            }

            return response.json();

        })

        .then(data =>
        {

            const currentTemperature =
                Number(data.temperature);

            document.getElementById(
                "temperature"
            ).textContent =
                currentTemperature.toFixed(2)
                + " °C";


            const currentHumidity =
                Number(data.humidity);

            document.getElementById(
                "humidity"
            ).textContent =
                currentHumidity.toFixed(2)
                + " %";


            const currentPressure =
                Number(data.pressure);

            document.getElementById(
                "pressure"
            ).textContent =
                currentPressure.toFixed(2)
                + " hPa";


            const currentGasResistance =
                Number(data.gasResistance);

            document.getElementById(
                "gasResistance"
            ).textContent =
                currentGasResistance.toFixed(2)
                + " kΩ";


            // -----------------------------------------
            // Air Quality
            // -----------------------------------------

            const airQualityScore =
                Number(data.airQualityScore);

            const airQualityStatus =
                data.airQualityStatus || "UNKNOWN";

            const airQualityMessage =
                data.airQualityMessage ||
                "Air quality data unavailable";


            const scoreElement =
                document.getElementById(
                    "airQualityScore"
                );

            const statusElement =
                document.getElementById(
                    "airQualityStatus"
                );

            const messageElement =
                document.getElementById(
                    "airQualityMessage"
                );


            if (Number.isFinite(airQualityScore))
            {
                scoreElement.textContent =
                    airQualityScore.toFixed(1) +
                    " / 100";
            }
            else
            {
                scoreElement.textContent =
                    "-- / 100";
            }

            const aqRing = document.getElementById("aqRing");
            const aqRingValue = document.getElementById("aqRingValue");
            if (Number.isFinite(airQualityScore))
            {
                const pct = Math.max(0, Math.min(100, airQualityScore));
                aqRing.style.setProperty("--aq-pct", pct.toFixed(1));
                aqRingValue.textContent = pct.toFixed(1);
            }
            else
            {
                aqRing.style.setProperty("--aq-pct", "0");
                aqRingValue.textContent = "--";
            }

            document.getElementById("aqDetails").textContent =
                "Relative index • baseline " +
                (Number.isFinite(Number(data.airQualityBaseline)) ? Number(data.airQualityBaseline).toFixed(2) : "--") +
                " kΩ • raw " +
                (Number.isFinite(Number(data.airQualityRawScore)) ? Number(data.airQualityRawScore).toFixed(1) : "--");

            document.getElementById("aqBaseline").textContent =
                Number.isFinite(Number(data.airQualityBaseline))
                    ? Number(data.airQualityBaseline).toFixed(2) + " kΩ"
                    : "--";


            statusElement.textContent =
                airQualityStatus;


            messageElement.textContent =
                airQualityMessage;


            // Reset status class before applying
            // the current Air Quality level.
            statusElement.className =
                "air-quality-status";


            const normalizedStatus =
                airQualityStatus
                    .toUpperCase()
                    .trim();


            if (normalizedStatus === "GOOD")
            {
                statusElement.classList.add(
                    "aq-good"
                );
            }
            else if (
                normalizedStatus === "MODERATE"
            )
            {
                statusElement.classList.add(
                    "aq-moderate"
                );
            }
            else if (
                normalizedStatus === "POOR"
            )
            {
                statusElement.classList.add(
                    "aq-poor"
                );
            }
            else if (
                normalizedStatus === "UNHEALTHY"
            )
            {
                statusElement.classList.add(
                    "aq-unhealthy"
                );
            }


            document.getElementById(
                "sensorUpdated"
            ).textContent =
                "Updated: "
                + new Date().toLocaleTimeString();


            document.getElementById(
                "sensorError"
            ).style.display = "none";

        })

        .catch(error =>
        {

            console.log(
                "Sensor API Error:",
                error
            );

            document.getElementById(
                "sensorError"
            ).style.display = "block";

        });

}


// =====================================================
// System Data
// =====================================================

function updateSystemData()
{

    fetch("/api/system")

        .then(response =>
        {

            if (!response.ok)
            {
                throw new Error("System HTTP error");
            }

            return response.json();

        })

        .then(data =>
        {

            const wifiStatus =
                document.getElementById(
                    "wifiStatus"
                );


            // -----------------------------------------
            // Wi-Fi Status
            // -----------------------------------------

            if (data.wifi === true)
            {

                wifiStatus.textContent =
                    "● Connected";

                wifiStatus.className =
                    "status connected";

            }
            else
            {

                wifiStatus.textContent =
                    "● Disconnected";

                wifiStatus.className =
                    "status disconnected";

            }


            // -----------------------------------------
            // SSID
            // -----------------------------------------

            document.getElementById(
                "ssid"
            ).textContent =
                data.ssid || "--";


            // -----------------------------------------
            // IP
            // -----------------------------------------

            document.getElementById(
                "ipAddress"
            ).textContent =
                data.ip || "--";


            // -----------------------------------------
            // RSSI
            // -----------------------------------------

            document.getElementById(
                "rssi"
            ).textContent =
                data.rssi + " dBm";


            // -----------------------------------------
            // Firmware
            // -----------------------------------------

            document.getElementById(
                "firmware"
            ).textContent =
                data.firmware || "--";


            // -----------------------------------------
            // Uptime
            // -----------------------------------------

            document.getElementById(
                "uptime"
            ).textContent =
                formatUptime(
                    Number(data.uptime)
                );


            // -----------------------------------------
            // Diagnostics
            // -----------------------------------------

            document.getElementById("freeHeap").textContent =
                Math.round(Number(data.freeHeap || 0) / 1024) + " KB";

            document.getElementById("sensorReads").textContent =
                Number(data.sensorReads || 0).toLocaleString();

            document.getElementById("sensorFailures").textContent =
                Number(data.sensorFailures || 0).toLocaleString();

            document.getElementById("sensorRecoveries").textContent =
                Number(data.sensorRecoveries || 0).toLocaleString();

            document.getElementById("sensorOfflineEvents").textContent =
                Number(data.sensorOfflineEvents || 0).toLocaleString();

            document.getElementById("wifiDisconnects").textContent =
                Number(data.wifiDisconnects || 0).toLocaleString();

            document.getElementById("wifiReconnects").textContent =
                Number(data.wifiReconnectSuccess || 0).toLocaleString();

            document.getElementById("tcpConnections").textContent =
                Number(data.tcpLogConnections || 0).toLocaleString();

            document.getElementById("tcpDisconnects").textContent =
                Number(data.tcpLogDisconnects || 0).toLocaleString();

            document.getElementById("historySamples").textContent =
                Number(data.historySamples || 0) + " / " +
                Number(data.historyCapacity || 0);

            document.getElementById("historyCapacity").textContent =
                Number(data.historyCapacity || 0) + " samples";

            document.getElementById("historyInterval").textContent =
                Math.round(Number(data.historySampleIntervalSeconds || 0) / 60) + " min";

            document.getElementById("diagnosticFooter").textContent =
                "Sensor: " + (data.sensorHealthy ? "ONLINE" : "OFFLINE") +
                " • TCP: " + (data.tcpLogClient ? "CONNECTED" : "WAITING") +
                " • Updated: " + new Date().toLocaleTimeString();


            // -----------------------------------------
            // Device Health
            // -----------------------------------------

            const health =
                document.getElementById(
                    "health"
                );


            if (data.wifi === true && data.sensorHealthy === true && data.sensorOffline !== true)
            {
                health.textContent =
                    "● System Healthy";

                health.className =
                    "health healthy";

            }
            else if (data.wifi !== true)
            {
                health.textContent =
                    "● Wi-Fi Disconnected";

                health.className =
                    "health unhealthy";

            }
            else
            {
                health.textContent =
                    "● Sensor Requires Attention";

                health.className =
                    "health unhealthy";

            }


            document.getElementById(
                "systemUpdated"
            ).textContent =
                "Updated: "
                + new Date().toLocaleTimeString();


            document.getElementById(
                "systemError"
            ).style.display = "none";

        })

        .catch(error =>
        {

            console.log(
                "System API Error:",
                error
            );


            document.getElementById(
                "systemError"
            ).style.display = "block";


            const health =
                document.getElementById(
                    "health"
                );


            health.textContent =
                "● System Communication Error";

            health.className =
                "health unhealthy";

        });

}


// =====================================================
// Initial Update
// =====================================================

updateSensorData();

updateSystemData();

loadHistory();


// =====================================================
// Automatic Updates
// =====================================================

setInterval(
    updateSensorData,
    2000
);

setInterval(
    updateSystemData,
    2000
);

setInterval(
    loadHistory,
    30000
);

</script>


</body>

</html>

)rawliteral";


    server.send(
        200,
        "text/html; charset=UTF-8",
        page
    );
}
// =====================================================
// Wi-Fi Scan
// =====================================================

void handleWiFiScan()
{
    Serial.println();
    Serial.println("Scanning Wi-Fi networks...");

    int networkCount = WiFi.scanNetworks(false, true);

    String json = "[";

    for (int i = 0; i < networkCount; i++)
    {
        if (i > 0)
        {
            json += ",";
        }

        String ssid = WiFi.SSID(i);

        // Escape characters required for JSON
        ssid.replace("\\", "\\\\");
        ssid.replace("\"", "\\\"");

        json += "{";

        json += "\"ssid\":\"";
        json += ssid;
        json += "\",";

        json += "\"rssi\":";
        json += String(WiFi.RSSI(i));

        json += "}";
    }

    json += "]";

    server.send(
        200,
        "application/json",
        json
    );

    WiFi.scanDelete();

    Serial.print("Networks Found: ");
    Serial.println(networkCount);
}

void handleWiFiConnect()
{
    Serial.println();
    Serial.println("==============================");
    Serial.println("Wi-Fi Connection Attempt");
    Serial.println("==============================");

    if (!server.hasArg("ssid") ||
        !server.hasArg("password"))
    {
        server.send(
            400,
            "application/json",
            "{\"success\":false,\"message\":\"Missing Wi-Fi credentials\"}"
        );

        return;
    }

    String ssid =
        server.arg("ssid");

    String password =
        server.arg("password");


    Serial.print("SSID: ");
    Serial.println(ssid);

    Serial.println("Password: ********");

    Serial.println("Connecting...");


    // Keep Setup AP running while attempting
    // the connection.
    WiFi.mode(WIFI_AP_STA);

    WiFi.begin(
        ssid.c_str(),
        password.c_str()
    );


    unsigned long startTime =
        millis();


    const unsigned long timeout =
        15000;


    while (
        WiFi.status() != WL_CONNECTED &&
        millis() - startTime < timeout
    )
    {
        delay(250);

        Serial.print(".");
    }


    Serial.println();


    if (WiFi.status() == WL_CONNECTED)
    {
        Serial.println("Wi-Fi Connected!");

        Serial.print("IP Address: ");
        Serial.println(WiFi.localIP());
            // Save credentials only after successful connection
    if (!saveWiFiCredentials(ssid, password))
    {
        Serial.println("Failed to save WiFi credentials");

        server.send(
            500,
            "application/json",
            "{\"success\":false,\"message\":\"Wi-Fi connected but credentials could not be saved\"}"
        );

        return;
    }

    Serial.println("Wi-Fi Credentials Saved");


        String json = "{";

        json += "\"success\":true,";
        json += "\"message\":\"Wi-Fi connected\",";
        json += "\"ip\":\"";
        json += WiFi.localIP().toString();
        json += "\"";

        json += "}";


        server.send(
            200,
            "application/json",
            json
        );

        return;
    }


    Serial.println("Wi-Fi Connection Failed");


    WiFi.disconnect();


    server.send(
        200,
        "application/json",
        "{\"success\":false,\"message\":\"Unable to connect to selected Wi-Fi\"}"
    );
}


// =====================================================
// Initialize Setup Portal
// =====================================================

bool initSetupPortal()
{
    Serial.println();
    Serial.println("==============================");
    Serial.println("Starting Setup Portal");
    Serial.println("==============================");

    // Start ESP32 Access Point
    WiFi.mode(WIFI_AP);

    if (!WiFi.softAP("AirSense-Pro", "airsense123"))
    {
        Serial.println("Failed to start Access Point");

        return false;
    }

    Serial.print("AP IP : ");
    Serial.println(WiFi.softAPIP());


    // Root page
    server.on(
        "/",
        HTTP_GET,
        handleRoot
    );


    // Wi-Fi scan API
    server.on(
        "/scan",
        HTTP_GET,
        handleWiFiScan
    );

    //WiFI connect API
    server.on(
    "/connect",
    HTTP_POST,
    handleWiFiConnect
    );


    // Start HTTP server
    server.begin();

    portalRunning = true;

    Serial.println("Setup Portal Ready");

    return true;
}
//Dasboard Initialisation
bool initWebServer()
{
    if (!isWiFiConnected())
    {
        Serial.println("Cannot start Web Server - Wi-Fi not connected");

        return false;
    }

    Serial.println();
    Serial.println("==============================");
    Serial.println("Starting Web Server");
    Serial.println("==============================");

    server.on(
        "/",
        HTTP_GET,
        handleDashboard
    );
    
    server.on(
    "/api/sensor",
    HTTP_GET,
    handleSensorAPI
    );

    server.on(
    "/api/system",
    HTTP_GET,
    handleSystemAPI
);

    server.on(
    "/api/history",
    HTTP_GET,
    handleHistoryAPI
);

    server.begin();

    portalRunning = true;

    Serial.println("Web Server Ready");

    Serial.print("Dashboard IP : ");
    Serial.println(WiFi.localIP());

    return true;
}


// =====================================================
// Update Setup Portal
// =====================================================

void updateSetupPortal()
{
    if (portalRunning)
    {
        server.handleClient();
    }
}


// =====================================================
// Portal Status
// =====================================================

bool isSetupPortalRunning()
{
    return portalRunning;
}