#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include <WebServer.h>
#include "MAX30105.h"

#include "../lib/Filter/Filter.h"
#include "../lib/HeartRate/HeartRate.h"
#include "../lib/SpO2/SpO2.h"

MAX30105 sensor;
MedianFilter medianRed, medianIR;
MeanFilter meanRed, meanIR;
HeartRateDetector hrDetector;
SpO2Calculator spo2Calc;

WebServer server(80);

static const char *AP_SSID = "ESP32-HealthDemo";
static const char *AP_PASS = "12345678";

static const uint32_t SENSOR_INTERVAL_MS = 20;  // 50 Hz
static const uint32_t NO_FINGER_THRESHOLD = 50000;
static const int SAMPLE_BUFFER_SIZE = 180;

uint32_t redBuffer[SAMPLE_BUFFER_SIZE] = {0};
uint32_t irBuffer[SAMPLE_BUFFER_SIZE] = {0};
unsigned long tsBuffer[SAMPLE_BUFFER_SIZE] = {0};
int sampleWriteIndex = 0;
int sampleCount = 0;

int currentBPM = 0;
int currentSpO2 = 0;
int beatCount = 0;
bool fingerPresent = false;
unsigned long lastBeatMs = 0;
unsigned long lastSampleMs = 0;

const char INDEX_HTML[] PROGMEM = R"HTML(
<!doctype html>
<html lang="zh-CN">
<head>
  <meta charset="utf-8" />
  <meta name="viewport" content="width=device-width, initial-scale=1" />
  <title>ESP32 Health Demo</title>
  <style>
    :root {
      --bg: #081523;
      --panel: #10263c;
      --text: #ebf6ff;
      --muted: #9ec1dd;
      --accent: #33d69f;
      --red: #ff6474;
      --ir: #8f81ff;
    }
    * { box-sizing: border-box; }
    body {
      margin: 0;
      font-family: "Noto Sans SC", "PingFang SC", "Microsoft YaHei", sans-serif;
      background: radial-gradient(circle at 20% 0%, #143456 0%, var(--bg) 58%);
      color: var(--text);
      padding: 16px;
    }
    .wrap {
      max-width: 840px;
      margin: 0 auto;
      display: grid;
      gap: 12px;
    }
    .panel {
      background: linear-gradient(160deg, #12324f, var(--panel));
      border: 1px solid #2f4f6c;
      border-radius: 14px;
      padding: 12px;
      box-shadow: 0 10px 24px rgba(0,0,0,.24);
    }
    .head {
      display: flex;
      justify-content: space-between;
      align-items: center;
      gap: 8px;
      flex-wrap: wrap;
    }
    .metrics {
      display: flex;
      gap: 12px;
      flex-wrap: wrap;
    }
    .metric {
      min-width: 135px;
      background: rgba(8, 21, 35, .45);
      border: 1px solid #35516b;
      border-radius: 10px;
      padding: 10px;
    }
    .k { color: var(--muted); font-size: 12px; }
    .v { font-size: 28px; font-weight: 700; margin-top: 3px; }
    .ok { color: var(--accent); }
    .warn { color: #ffd26a; }
    canvas {
      width: 100%;
      height: 220px;
      background: #0a1f33;
      border-radius: 10px;
      border: 1px solid #284761;
      display: block;
      margin-top: 10px;
    }
    .legend {
      margin-top: 8px;
      color: var(--muted);
      font-size: 12px;
      display: flex;
      gap: 12px;
      flex-wrap: wrap;
    }
    .dot {
      display: inline-block;
      width: 10px;
      height: 10px;
      border-radius: 50%;
      margin-right: 5px;
    }
    .footer {
      color: #89afcc;
      font-size: 12px;
      opacity: .9;
    }
  </style>
</head>
<body>
  <div class="wrap">
    <div class="panel">
      <div class="head">
        <div>
          <div style="font-size:18px;font-weight:700">ESP32 + MAX30102 Mobile Demo</div>
          <div class="k">将手指轻放在传感器上，保持 5-10 秒观察稳定数据</div>
        </div>
        <div id="status" class="warn">状态: 等待手指</div>
      </div>
      <div class="metrics">
        <div class="metric">
          <div class="k">Heart Rate</div>
          <div class="v" id="bpm">--</div>
        </div>
        <div class="metric">
          <div class="k">SpO2</div>
          <div class="v" id="spo2">--</div>
        </div>
        <div class="metric">
          <div class="k">Beat Count</div>
          <div class="v" id="beats">0</div>
        </div>
      </div>
      <canvas id="wave" width="800" height="220"></canvas>
      <div class="legend">
        <span><i class="dot" style="background:#ff6474"></i>Red PPG</span>
        <span><i class="dot" style="background:#8f81ff"></i>IR PPG</span>
      </div>
    </div>
    <div class="footer">AP SSID: ESP32-HealthDemo, URL: http://192.168.4.1</div>
  </div>
  <script>
    const bpmEl = document.getElementById('bpm');
    const spo2El = document.getElementById('spo2');
    const beatsEl = document.getElementById('beats');
    const statusEl = document.getElementById('status');
    const canvas = document.getElementById('wave');
    const ctx = canvas.getContext('2d');

    function drawLines(red, ir) {
      ctx.clearRect(0, 0, canvas.width, canvas.height);
      if (!red.length || !ir.length) return;

      const all = red.concat(ir);
      const min = Math.min(...all);
      const max = Math.max(...all);
      const span = Math.max(1, max - min);

      const mapY = (v) => canvas.height - 16 - ((v - min) / span) * (canvas.height - 32);
      const stepX = (canvas.width - 24) / Math.max(1, red.length - 1);

      const draw = (arr, color) => {
        ctx.beginPath();
        ctx.strokeStyle = color;
        ctx.lineWidth = 2;
        for (let i = 0; i < arr.length; i++) {
          const x = 12 + i * stepX;
          const y = mapY(arr[i]);
          if (i === 0) ctx.moveTo(x, y);
          else ctx.lineTo(x, y);
        }
        ctx.stroke();
      };

      draw(red, '#ff6474');
      draw(ir, '#8f81ff');
    }

    async function refresh() {
      try {
        const res = await fetch('/api/data', { cache: 'no-store' });
        const d = await res.json();

        bpmEl.textContent = d.bpm > 0 ? d.bpm : '--';
        spo2El.textContent = d.spo2 > 0 ? d.spo2 + '%' : '--';
        beatsEl.textContent = d.beats;

        if (d.finger) {
          statusEl.textContent = d.beat ? '状态: 检测到心跳' : '状态: 采集中';
          statusEl.className = 'ok';
        } else {
          statusEl.textContent = '状态: 等待手指';
          statusEl.className = 'warn';
        }

        drawLines(d.red || [], d.ir || []);
      } catch (e) {
        statusEl.textContent = '状态: 连接中断，重试中...';
        statusEl.className = 'warn';
      }
    }

    refresh();
    setInterval(refresh, 180);
  </script>
</body>
</html>
)HTML";

void resetPipelineState() {
  hrDetector.reset();
  spo2Calc.reset();
  medianRed.reset();
  medianIR.reset();
  meanRed.reset();
  meanIR.reset();
  currentBPM = 0;
  currentSpO2 = 0;
  beatCount = 0;
  sampleCount = 0;
  sampleWriteIndex = 0;
}

void pushSample(unsigned long ts, uint32_t red, uint32_t ir) {
  tsBuffer[sampleWriteIndex] = ts;
  redBuffer[sampleWriteIndex] = red;
  irBuffer[sampleWriteIndex] = ir;
  sampleWriteIndex = (sampleWriteIndex + 1) % SAMPLE_BUFFER_SIZE;
  if (sampleCount < SAMPLE_BUFFER_SIZE) {
    sampleCount++;
  }
}

void handleRoot() {
  server.send_P(200, "text/html; charset=utf-8", INDEX_HTML);
}

void handleData() {
  const bool beatActive = (millis() - lastBeatMs) < 300;
  String json;
  json.reserve(7200);
  json += "{\"finger\":";
  json += fingerPresent ? "1" : "0";
  json += ",\"bpm\":";
  json += String(currentBPM);
  json += ",\"spo2\":";
  json += String(currentSpO2);
  json += ",\"beats\":";
  json += String(beatCount);
  json += ",\"beat\":";
  json += beatActive ? "1" : "0";
  json += ",\"red\":[";

  const int oldest = (sampleWriteIndex + SAMPLE_BUFFER_SIZE - sampleCount) % SAMPLE_BUFFER_SIZE;
  for (int i = 0; i < sampleCount; i++) {
    if (i > 0) json += ",";
    const int idx = (oldest + i) % SAMPLE_BUFFER_SIZE;
    json += String(redBuffer[idx]);
  }

  json += "],\"ir\":[";
  for (int i = 0; i < sampleCount; i++) {
    if (i > 0) json += ",";
    const int idx = (oldest + i) % SAMPLE_BUFFER_SIZE;
    json += String(irBuffer[idx]);
  }
  json += "]}";

  server.send(200, "application/json; charset=utf-8", json);
}

void setupWebServer() {
  server.on("/", HTTP_GET, handleRoot);
  server.on("/api/data", HTTP_GET, handleData);
  server.begin();
}

void setupSensor() {
  if (!sensor.begin(Wire, I2C_SPEED_STANDARD)) {
    Serial.println("[ERROR] MAX30102 not found. Check wiring.");
    while (true) {
      delay(1000);
    }
  }

  byte ledBrightness = 60;
  byte sampleAverage = 4;
  byte ledMode = 2;
  int sampleRate = 100;
  int pulseWidth = 411;
  int adcRange = 4096;
  sensor.setup(ledBrightness, sampleAverage, ledMode, sampleRate, pulseWidth, adcRange);
}

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 3000) {
    delay(10);
  }

  Serial.println();
  Serial.println("=== ESP32 Mobile Web Demo ===");

  setupSensor();
  resetPipelineState();

  WiFi.mode(WIFI_AP);
  const bool apOk = WiFi.softAP(AP_SSID, AP_PASS);
  if (!apOk) {
    Serial.println("[ERROR] SoftAP start failed.");
  }

  IPAddress ip = WiFi.softAPIP();
  Serial.printf("AP SSID: %s\n", AP_SSID);
  Serial.printf("AP PASS: %s\n", AP_PASS);
  Serial.printf("Open URL: http://%s\n", ip.toString().c_str());

  setupWebServer();
}

void loop() {
  server.handleClient();

  const unsigned long now = millis();
  if (now - lastSampleMs < SENSOR_INTERVAL_MS) {
    delay(2);
    return;
  }
  lastSampleMs = now;

  const uint32_t rawRed = sensor.getRed();
  const uint32_t rawIR = sensor.getIR();

  if (rawIR < NO_FINGER_THRESHOLD) {
    if (fingerPresent) {
      fingerPresent = false;
      resetPipelineState();
    }
    return;
  }

  fingerPresent = true;

  const uint32_t medRed = medianRed.update(rawRed);
  const uint32_t medIR = medianIR.update(rawIR);
  const uint32_t filteredRed = meanRed.update(medRed);
  const uint32_t filteredIR = meanIR.update(medIR);

  const bool beatDetected = hrDetector.update(filteredIR);
  spo2Calc.update(filteredRed, filteredIR);
  if (beatDetected) {
    spo2Calc.onBeatDetected();
    beatCount++;
    lastBeatMs = now;
  }

  const int bpm = hrDetector.getBPM();
  const int spo2 = spo2Calc.getSpO2();
  currentBPM = bpm > 0 ? bpm : 0;
  currentSpO2 = spo2 > 0 ? spo2 : 0;

  pushSample(now, filteredRed, filteredIR);
}
