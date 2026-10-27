#include "WebOTAManager.h"
#include "Settings.h"
#include <ESP8266WiFi.h>

WebOTAManager::WebOTAManager(DisplayManager &displayRef)
    : display(displayRef), httpServer(80)
{
}

void WebOTAManager::initialize()
{
  // Setup HTTP update server
  httpUpdater.setup(&httpServer, "/update", "admin", "mqtt-clock-web");

  // Setup web pages
  httpServer.on("/", [this]()
                { handleRoot(); });
  httpServer.on("/status", [this]()
                { handleStatus(); });
  httpServer.on("/info", [this]()
                { handleInfo(); });

  httpServer.begin();

  Serial.println(F("Web OTA Server started"));
  Serial.print(F("Update URL: http://"));
  Serial.print(WiFi.localIP());
  Serial.println(F("/update"));
  Serial.println(F("Username: admin"));
  Serial.println(F("Password: mqtt-clock-web"));
}

void WebOTAManager::loop()
{
  httpServer.handleClient();
}

void WebOTAManager::handleRoot()
{
  // The charset must be declared or the browser falls back to Latin-1 and the
  // UTF-8 emoji in the page render as mojibake ("ðŸ•" instead of "🕐").
  httpServer.send(200, "text/html; charset=UTF-8", getIndexHTML());
}

void WebOTAManager::handleStatus()
{
  httpServer.send(200, "text/html; charset=UTF-8", getStatusHTML());
}

void WebOTAManager::handleInfo()
{
  String json = "{";
  json += "\"device\":\"MQTT Clock\",";
  json += "\"ip\":\"" + WiFi.localIP().toString() + "\",";
  json += "\"mac\":\"" + WiFi.macAddress() + "\",";
  json += "\"hostname\":\"" + String(DEVICE_HOSTNAME) + "\",";
  json += "\"uptime\":" + String(millis() / 1000) + ",";
  json += "\"free_heap\":" + String(ESP.getFreeHeap()) + ",";
  json += "\"chip_id\":\"" + String(ESP.getChipId(), HEX) + "\",";
  json += "\"flash_size\":" + String(ESP.getFlashChipSize()) + ",";
  json += "\"sdk_version\":\"" + String(ESP.getSdkVersion()) + "\"";
  json += "}";

  httpServer.send(200, "application/json", json);
}

// The page templates live in flash (PROGMEM). As ordinary string literals the
// ESP8266 copies them into its ~80 KB of RAM at boot and keeps them there for
// good, even though they are only needed while a page is being served.
// %TOKENS% are filled in per request; the MQTT topics are compile-time
// constants, so they are spliced in by literal concatenation instead.
static const char INDEX_HTML[] PROGMEM = R"(
<!DOCTYPE html>
<html>
<head>
    <title>MQTT Clock Control</title>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <style>
        body { font-family: Arial; margin: 20px; background: #f0f0f0; }
        .container { max-width: 600px; margin: 0 auto; background: white; padding: 20px; border-radius: 10px; box-shadow: 0 4px 6px rgba(0,0,0,0.1); }
        h1 { color: #2c3e50; text-align: center; }
        .card { background: #ecf0f1; padding: 15px; margin: 10px 0; border-radius: 5px; }
        .button { background: #3498db; color: white; padding: 10px 20px; border: none; border-radius: 5px; cursor: pointer; margin: 5px; text-decoration: none; display: inline-block; }
        .button:hover { background: #2980b9; }
        .update-btn { background: #e74c3c; }
        .update-btn:hover { background: #c0392b; }
        .info { background: #f39c12; color: white; padding: 10px; border-radius: 5px; margin: 10px 0; }
    </style>
</head>
<body>
    <div class="container">
        <h1>🕐 MQTT Clock Control</h1>

        <div class="card">
            <h3>📡 Device Information</h3>
            <p><strong>IP Address:</strong> %IP%</p>
            <p><strong>Hostname:</strong> %HOSTNAME%</p>
            <p><strong>MAC Address:</strong> %MAC%</p>
            <p><strong>Uptime:</strong> %UPTIME% seconds</p>
        </div>

        <div class="card">
            <h3>🔄 Firmware Update</h3>
            <div class="info">
                <strong>⚠️ Warning:</strong> Make sure you have the correct firmware file (.bin) before updating!
            </div>
            <a href="/update" class="button update-btn">🚀 Update Firmware</a>
            <p><small>Username: <code>admin</code> | Password: <code>mqtt-clock-web</code></small></p>
        </div>

        <div class="card">
            <h3>📊 System Status</h3>
            <a href="/status" class="button">📈 View Status</a>
            <a href="/info" class="button">ℹ️ JSON Info</a>
        </div>

        <div class="card">
            <h3>📋 MQTT Topics</h3>
            <p><strong>Notifications:</strong> <code>)" MQTT_TOPIC_PREFIX_LITERAL R"(/notification</code></p>
            <p><strong>Animation:</strong> <code>)" MQTT_TOPIC_PREFIX_LITERAL R"(/animation</code></p>
            <p><strong>Day Brightness:</strong> <code>)" MQTT_TOPIC_PREFIX_LITERAL R"(/brightness/day</code></p>
            <p><strong>Night Brightness:</strong> <code>)" MQTT_TOPIC_PREFIX_LITERAL R"(/brightness/night</code></p>
            <p><strong>Day Start:</strong> <code>)" MQTT_TOPIC_PREFIX_LITERAL R"(/schedule/day_start</code></p>
            <p><strong>Night Start:</strong> <code>)" MQTT_TOPIC_PREFIX_LITERAL R"(/schedule/night_start</code></p>
            <p><strong>Status:</strong> <code>)" MQTT_TOPIC_PREFIX_LITERAL R"(/status</code></p>
        </div>
    </div>
</body>
</html>
)";

static const char STATUS_HTML[] PROGMEM = R"(
<!DOCTYPE html>
<html>
<head>
    <title>MQTT Clock Status</title>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <meta http-equiv="refresh" content="5">
    <style>
        body { font-family: Arial; margin: 20px; background: #f0f0f0; }
        .container { max-width: 600px; margin: 0 auto; background: white; padding: 20px; border-radius: 10px; }
        h1 { color: #2c3e50; text-align: center; }
        .status { background: #2ecc71; color: white; padding: 10px; border-radius: 5px; margin: 10px 0; text-align: center; }
        .metric { display: flex; justify-content: space-between; padding: 10px; border-bottom: 1px solid #ecf0f1; }
        .value { font-weight: bold; color: #3498db; }
    </style>
</head>
<body>
    <div class="container">
        <h1>📊 System Status</h1>
        <div class="status">🟢 MQTT Clock Online</div>

        <div class="metric">
            <span>Free Heap Memory:</span>
            <span class="value">%FREE_HEAP% bytes</span>
        </div>

        <div class="metric">
            <span>Uptime:</span>
            <span class="value">%UPTIME% seconds</span>
        </div>

        <div class="metric">
            <span>WiFi Signal:</span>
            <span class="value">%RSSI% dBm</span>
        </div>

        <div class="metric">
            <span>Flash Size:</span>
            <span class="value">%FLASH_SIZE% bytes</span>
        </div>

        <div class="metric">
            <span>SDK Version:</span>
            <span class="value">%SDK_VERSION%</span>
        </div>

        <p><a href="/">← Back to Home</a></p>
        <p><small>Auto-refresh every 5 seconds</small></p>
    </div>
</body>
</html>
)";

String WebOTAManager::getIndexHTML()
{
  String html = FPSTR(INDEX_HTML);
  html.replace(F("%IP%"), WiFi.localIP().toString());
  html.replace(F("%HOSTNAME%"), DEVICE_HOSTNAME);
  html.replace(F("%MAC%"), WiFi.macAddress());
  html.replace(F("%UPTIME%"), String(millis() / 1000));
  return html;
}

String WebOTAManager::getStatusHTML()
{
  String html = FPSTR(STATUS_HTML);
  html.replace(F("%FREE_HEAP%"), String(ESP.getFreeHeap()));
  html.replace(F("%UPTIME%"), String(millis() / 1000));
  html.replace(F("%RSSI%"), String(WiFi.RSSI()));
  html.replace(F("%FLASH_SIZE%"), String(ESP.getFlashChipSize()));
  html.replace(F("%SDK_VERSION%"), ESP.getSdkVersion());
  return html;
}
