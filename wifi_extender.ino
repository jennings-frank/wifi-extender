/*
 * MIT License
 *
 * Copyright (c) 2026 Frank Jennings
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <Preferences.h>
#include "esp_wifi.h"

// ================= HARDWARE CONFIGURATION =================
#define RGB_PIN 48         // Try 38 if unlit on your board revision
#define BOOT_BTN_PIN 0     // Onboard BOOT button (hold 3s to factory reset)
#define LED_BRIGHTNESS 25
// ==========================================================

Preferences prefs;
WebServer server(80);
DNSServer dnsServer;

String sta_ssid = "";
String sta_pass = "";
String ap_ssid  = "Skullcave";
String ap_pass  = "skullcave";
String lastError = "";
volatile uint8_t lastDisconnectReason = 0;

bool isSetupMode = false;
bool isTurboMode = false;

IPAddress ap_ip(192, 168, 50, 1);
IPAddress ap_gateway(192, 168, 50, 1);
IPAddress ap_subnet(255, 255, 255, 0);
IPAddress ap_lease_start(192, 168, 50, 2);
IPAddress ap_dns(1, 1, 1, 1);

void setStatusLED(uint8_t r, uint8_t g, uint8_t b, uint8_t brightness) {
  rgbLedWrite(RGB_PIN,
              (r * brightness) / 255,
              (g * brightness) / 255,
              (b * brightness) / 255);
}

void setPerformanceProfile(bool turbo) {
  if (turbo && !isTurboMode) {
    esp_wifi_set_ps(WIFI_PS_NONE);
    WiFi.setTxPower(WIFI_POWER_19_5dBm);
    isTurboMode = true;
  } else if (!turbo && isTurboMode) {
    WiFi.setTxPower(WIFI_POWER_8_5dBm);
    esp_wifi_set_ps(WIFI_PS_MIN_MODEM);
    isTurboMode = false;
  }
}

void onWiFiEvent(WiFiEvent_t event, WiFiEventInfo_t info) {
  if (event == ARDUINO_EVENT_WIFI_STA_DISCONNECTED) {
    lastDisconnectReason = info.wifi_sta_disconnected.reason;
  }
}

// ================= WEB GUI HTML & API =================
const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>Skullcave Wi-Fi Extender</title>
<style>
  :root {
    --bg: #f4f6f9;
    --card: #ffffff;
    --text: #1e293b;
    --muted: #64748b;
    --accent: #2563eb;
    --accent-hover: #1d4ed8;
    --border: #e2e8f0;
  }
  * { box-sizing: border-box; font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif; }
  body { margin: 0; padding: 16px; background: var(--bg); color: var(--text); display: flex; justify-content: center; }
  .wrap { width: 100%; max-width: 440px; }
  .card { background: var(--card); border-radius: 16px; padding: 20px; margin-bottom: 16px; box-shadow: 0 4px 16px rgba(0,0,0,0.06); border: 1px solid var(--border); }
  h1 { font-size: 1.3rem; margin: 0 0 4px 0; }
  .sub { font-size: 0.85rem; color: var(--muted); margin-bottom: 16px; }
  .err { background: #fef2f2; color: #b91c1c; border: 1px solid #fecaca; padding: 10px 12px; border-radius: 10px; font-size: 0.84rem; margin-bottom: 14px; display: none; }
  .stats { display: grid; grid-template-columns: 1fr 1fr; gap: 10px; }
  .stat { background: var(--bg); padding: 12px; border-radius: 10px; }
  .stat-label { font-size: 0.72rem; text-transform: uppercase; color: var(--muted); letter-spacing: 0.04em; }
  .stat-val { font-size: 1.05rem; font-weight: 700; margin-top: 4px; }
  label { display: block; font-size: 0.82rem; font-weight: 600; margin: 12px 0 6px; }
  input { width: 100%; padding: 11px 12px; border: 1px solid var(--border); border-radius: 10px; font-size: 0.95rem; outline: none; }
  input:focus { border-color: var(--accent); }
  .btn { width: 100%; padding: 12px; border: none; border-radius: 10px; background: var(--accent); color: #fff; font-size: 0.95rem; font-weight: 600; cursor: pointer; margin-top: 16px; }
  .btn:hover { background: var(--accent-hover); }
  .btn:disabled { background: #94a3b8; cursor: default; }
  .btn-sec { background: #e2e8f0; color: var(--text); margin-top: 8px; }
  .net-list { max-height: 170px; overflow-y: auto; border: 1px solid var(--border); border-radius: 10px; margin-top: 8px; }
  .net-item { padding: 10px 12px; border-bottom: 1px solid var(--border); display: flex; justify-content: space-between; align-items: center; cursor: pointer; font-size: 0.9rem; }
  .net-item:last-child { border-bottom: none; }
  .net-item:hover { background: #eff6ff; }
  .badge { font-size: 0.75rem; padding: 2px 8px; border-radius: 99px; background: #e2e8f0; font-weight: 600; }
  .row { display: flex; align-items: center; gap: 8px; margin-top: 8px; font-size: 0.82rem; color: var(--muted); }
  .row input { width: auto; }
</style>
</head>
<body>
<div class="wrap">
  <div class="card">
    <h1>Skullcave Wi-Fi Extender</h1>
    <div class="sub" id="modeText">Loading telemetry...</div>
    <div class="err" id="errBox"></div>
    <div class="stats">
      <div class="stat"><div class="stat-label">Router Signal</div><div class="stat-val" id="rssi">--</div></div>
      <div class="stat"><div class="stat-label">Clients</div><div class="stat-val" id="clients">--</div></div>
      <div class="stat"><div class="stat-label">Power Profile</div><div class="stat-val" id="pwr">--</div></div>
      <div class="stat"><div class="stat-label">Channel / BW</div><div class="stat-val" id="chan">--</div></div>
    </div>
  </div>

  <div class="card">
    <h1>Configure Networks</h1>
    <div class="sub">Select your upstream router and customize your extender hotspot.</div>

    <button type="button" class="btn btn-sec" onclick="scanWifi()" id="scanBtn">Scan Nearby Wi-Fi Networks</button>
    <div class="net-list" id="netList" style="display:none;"></div>

    <div id="cfgForm">
      <label>Main Router SSID</label>
      <input type="text" id="sta_ssid" autocomplete="off" autocapitalize="none" required placeholder="Home_WiFi">

      <label>Main Router Password</label>
      <input type="password" id="sta_pass" autocomplete="off" autocapitalize="none" placeholder="Router Password">

      <label>Extender Hotspot Name (SSID)</label>
      <input type="text" id="ap_ssid" autocomplete="off" autocapitalize="none" required placeholder="Skullcave">

      <label>Extender Hotspot Password (min 8 chars)</label>
      <input type="password" id="ap_pass" minlength="8" autocomplete="off" autocapitalize="none" required placeholder="skullcave">

      <div class="row">
        <input type="checkbox" id="showPw" onclick="togglePw()">
        <label for="showPw" style="margin:0;font-weight:400;">Show passwords</label>
      </div>

      <button type="button" class="btn" onclick="saveConfig()" id="saveBtn">Save & Reboot Extender</button>
    </div>
  </div>
</div>

<script>
function togglePw() {
  const t = document.getElementById('showPw').checked ? 'text' : 'password';
  document.getElementById('sta_pass').type = t;
  document.getElementById('ap_pass').type = t;
}

async function loadStatus() {
  try {
    const r = await fetch('/api/status');
    const d = await r.json();
    document.getElementById('modeText').innerText = d.connected ? 'Connected to: ' + d.sta_ssid : 'Setup Mode (Not connected to router)';
    const errBox = document.getElementById('errBox');
    if (d.error) {
      errBox.innerText = d.error;
      errBox.style.display = 'block';
    } else {
      errBox.style.display = 'none';
    }
    document.getElementById('rssi').innerText = d.connected ? d.rssi + ' dBm' : 'N/A';
    document.getElementById('clients').innerText = d.clients + ' active';
    document.getElementById('pwr').innerText = d.turbo ? 'Turbo (19.5dBm)' : 'Eco-Idle (8.5dBm)';
    document.getElementById('chan').innerText = 'Ch ' + d.channel + ' / 40MHz';
    if (!document.getElementById('sta_ssid').value) document.getElementById('sta_ssid').value = d.sta_ssid;
    if (!document.getElementById('ap_ssid').value) document.getElementById('ap_ssid').value = d.ap_ssid;
  } catch (e) {}
}

async function scanWifi() {
  const btn = document.getElementById('scanBtn');
  const list = document.getElementById('netList');
  btn.innerText = 'Scanning...';
  btn.disabled = true;
  try {
    const r = await fetch('/api/scan');
    const nets = await r.json();
    list.innerHTML = '';
    nets.forEach(n => {
      const div = document.createElement('div');
      div.className = 'net-item';
      div.innerHTML = `<span>${n.ssid}</span><span class="badge">${n.rssi} dBm (Ch ${n.ch})</span>`;
      div.onclick = () => { document.getElementById('sta_ssid').value = n.ssid; };
      list.appendChild(div);
    });
    list.style.display = nets.length ? 'block' : 'none';
  } catch (e) {}
  btn.innerText = 'Scan Nearby Wi-Fi Networks';
  btn.disabled = false;
}

async function saveConfig() {
  const staSsid = document.getElementById('sta_ssid').value.trim();
  const staPass = document.getElementById('sta_pass').value;
  const apSsid  = document.getElementById('ap_ssid').value.trim();
  const apPass  = document.getElementById('ap_pass').value;

  if (!staSsid || !apSsid || apPass.length < 8) {
    alert('Please fill Router SSID, Extender SSID, and an Extender Password (min 8 characters).');
    return;
  }

  const btn = document.getElementById('saveBtn');
  btn.innerText = 'Saving & Rebooting...';
  btn.disabled = true;

  const payload = new URLSearchParams();
  payload.append('sta_ssid', staSsid);
  payload.append('sta_pass', staPass);
  payload.append('ap_ssid', apSsid);
  payload.append('ap_pass', apPass);

  try {
    await fetch('/save', { method: 'POST', body: payload });
  } catch (e) {}

  btn.innerText = 'Rebooted! Switch Wi-Fi to ' + apSsid;
}

loadStatus();
setInterval(loadStatus, 4000);
</script>
</body>
</html>
)rawliteral";

void handleRoot() {
  server.send_P(200, "text/html", INDEX_HTML);
}

void handleCaptivePortal() {
  server.sendHeader("Location", "http://192.168.50.1/", true);
  server.send(302, "text/plain", "");
}

void handleStatus() {
  bool conn = (WiFi.status() == WL_CONNECTED);
  String safeErr = lastError;
  safeErr.replace("\"", "'");
  String json = "{";
  json += "\"connected\":" + String(conn ? "true" : "false") + ",";
  json += "\"sta_ssid\":\"" + sta_ssid + "\",";
  json += "\"ap_ssid\":\"" + ap_ssid + "\",";
  json += "\"error\":\"" + safeErr + "\",";
  json += "\"rssi\":" + String(conn ? WiFi.RSSI() : 0) + ",";
  json += "\"clients\":" + String(WiFi.softAPgetStationNum()) + ",";
  json += "\"turbo\":" + String(isTurboMode ? "true" : "false") + ",";
  json += "\"channel\":" + String(WiFi.channel());
  json += "}";
  server.send(200, "application/json", json);
}

void handleScan() {
  int n = WiFi.scanNetworks();
  String json = "[";
  for (int i = 0; i < n; i++) {
    if (i > 0) json += ",";
    String s = WiFi.SSID(i);
    s.replace("\"", "\\\"");
    json += "{\"ssid\":\"" + s + "\",\"rssi\":" + String(WiFi.RSSI(i)) + ",\"ch\":" + String(WiFi.channel(i)) + "}";
  }
  json += "]";
  WiFi.scanDelete();
  server.send(200, "application/json", json);
}

void handleSave() {
  if (server.hasArg("sta_ssid") && server.hasArg("ap_ssid") && server.hasArg("ap_pass")) {
    String cleanStaSsid = server.arg("sta_ssid");
    cleanStaSsid.trim();
    String cleanApSsid = server.arg("ap_ssid");
    cleanApSsid.trim();

    prefs.begin("extender", false);
    prefs.putString("sta_ssid", cleanStaSsid);
    prefs.putString("sta_pass", server.arg("sta_pass"));
    prefs.putString("ap_ssid",  cleanApSsid);
    prefs.putString("ap_pass",  server.arg("ap_pass"));
    prefs.end();

    delay(400);
    server.send(200, "text/plain", "OK");
    delay(800);
    ESP.restart();
  } else {
    server.send(400, "text/plain", "Missing required fields.");
  }
}

void startWebServer() {
  server.on("/", HTTP_GET, handleRoot);
  server.on("/api/status", HTTP_GET, handleStatus);
  server.on("/api/scan", HTTP_GET, handleScan);
  server.on("/save", HTTP_POST, handleSave);

  server.on("/generate_204", HTTP_GET, handleCaptivePortal);
  server.on("/gen_204", HTTP_GET, handleCaptivePortal);
  server.on("/hotspot-detect.html", HTTP_GET, handleCaptivePortal);
  server.on("/connecttest.txt", HTTP_GET, handleCaptivePortal);
  server.onNotFound(handleCaptivePortal);

  server.begin();
}

// ================= CORE 0 TELEMETRY & BUTTON WATCHDOG =================
void monitorTask(void* parameter) {
  unsigned long btnPressStart = 0;
  unsigned long lastReconnectAttempt = 0;

  for (;;) {
    if (digitalRead(BOOT_BTN_PIN) == LOW) {
      if (btnPressStart == 0) btnPressStart = millis();
      if (millis() - btnPressStart > 3000) {
        setStatusLED(255, 0, 0, 80);
        prefs.begin("extender", false);
        prefs.clear();
        prefs.end();
        delay(500);
        ESP.restart();
      }
    } else {
      btnPressStart = 0;
    }

    if (isSetupMode) {
      setStatusLED(180, 0, 255, LED_BRIGHTNESS);
      vTaskDelay(pdMS_TO_TICKS(600));
      setStatusLED(0, 0, 0, 0);
      vTaskDelay(pdMS_TO_TICKS(600));
    } else if (WiFi.status() == WL_CONNECTED) {
      uint8_t clients = WiFi.softAPgetStationNum();
      int32_t rssi = WiFi.RSSI();

      if (clients > 0) {
        setPerformanceProfile(true);
        if (rssi > -62) {
          setStatusLED(0, 255, 0, LED_BRIGHTNESS);
        } else if (rssi > -72) {
          setStatusLED(255, 140, 0, LED_BRIGHTNESS);
        } else {
          setStatusLED(255, 0, 180, LED_BRIGHTNESS);
        }
        vTaskDelay(pdMS_TO_TICKS(500));
      } else {
        setPerformanceProfile(false);
        setStatusLED(0, 120, 255, 6);
        vTaskDelay(pdMS_TO_TICKS(1000));
        setStatusLED(0, 0, 0, 0);
        vTaskDelay(pdMS_TO_TICKS(2000));
      }
    } else {
      // Disconnected during active extender mode: flash Red, but only trigger reconnect every 15s
      setPerformanceProfile(true);
      setStatusLED(255, 0, 0, LED_BRIGHTNESS);
      vTaskDelay(pdMS_TO_TICKS(300));
      setStatusLED(0, 0, 0, 0);
      vTaskDelay(pdMS_TO_TICKS(300));

      if (millis() - lastReconnectAttempt > 15000) {
        lastReconnectAttempt = millis();
        WiFi.disconnect(false);
        WiFi.begin(sta_ssid.c_str(), sta_pass.c_str());
      }
    }
  }
}

void startSetupPortal() {
  isSetupMode = true;
  WiFi.disconnect(false);
  delay(200);
  WiFi.mode(WIFI_AP_STA);

  WiFi.AP.begin();
  WiFi.AP.config(ap_ip, ap_gateway, ap_subnet, ap_lease_start, ap_ip);
  WiFi.AP.create("Skullcave_Setup", NULL, 6, false, 4);

  dnsServer.start(53, "*", ap_ip);
  startWebServer();
}

bool connectWithRetry(const char* ssid, const char* pass) {
  for (int attempt = 1; attempt <= 2; attempt++) {
    lastDisconnectReason = 0;
    WiFi.begin(ssid, pass);

    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < 15000) {
      delay(300);
    }
    if (WiFi.status() == WL_CONNECTED) {
      return true;
    }
    WiFi.disconnect(false);
    delay(1000);
  }
  return false;
}

void setup() {
  Serial.begin(115200);
  pinMode(BOOT_BTN_PIN, INPUT_PULLUP);
  setStatusLED(0, 80, 255, LED_BRIGHTNESS); // Solid Blue while connecting to router

  WiFi.onEvent(onWiFiEvent);

  prefs.begin("extender", true);
  sta_ssid = prefs.getString("sta_ssid", "");
  sta_pass = prefs.getString("sta_pass", "");
  ap_ssid  = prefs.getString("ap_ssid", "Skullcave");
  ap_pass  = prefs.getString("ap_pass", "skullcave");
  prefs.end();

  WiFi.mode(WIFI_AP_STA);
  WiFi.setMinSecurity(WIFI_AUTH_OPEN);
  WiFi.setTxPower(WIFI_POWER_19_5dBm);
  esp_wifi_set_ps(WIFI_PS_NONE);
  isTurboMode = true;

  if (sta_ssid.length() == 0) {
    startSetupPortal();
    xTaskCreatePinnedToCore(monitorTask, "NetMonitor", 4096, NULL, 1, NULL, 0);
    return;
  }

  if (!connectWithRetry(sta_ssid.c_str(), sta_pass.c_str())) {
    lastError = "Failed to join '" + sta_ssid + "' (status: " + String(WiFi.status()) + ", 802.11 reason: " + String(lastDisconnectReason) + ").";
    startSetupPortal();
    xTaskCreatePinnedToCore(monitorTask, "NetMonitor", 4096, NULL, 1, NULL, 0);
    return;
  }

  // Router connected cleanly! Now configure SoftAP + NAPT and launch Core 0 monitorTask
  lastError = "";
  int32_t routerChannel = WiFi.channel();
  esp_wifi_set_bandwidth(WIFI_IF_STA, WIFI_BW_HT40);
  esp_wifi_set_bandwidth(WIFI_IF_AP, WIFI_BW_HT40);

  WiFi.AP.begin();
  WiFi.AP.config(ap_ip, ap_gateway, ap_subnet, ap_lease_start, ap_dns);
  WiFi.AP.create(ap_ssid.c_str(), ap_pass.c_str(), routerChannel, false, 4);

  delay(600);
  WiFi.AP.enableNAPT(true);
  startWebServer();

  xTaskCreatePinnedToCore(monitorTask, "NetMonitor", 4096, NULL, 1, NULL, 0);
}

void loop() {
  if (isSetupMode) {
    dnsServer.processNextRequest();
  }
  server.handleClient();
  delay(2);
}
