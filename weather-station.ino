// ============================================================
//  ESP32 Weather Station
//  Sensors : DHT11 (GPIO 4)
//  Cloud   : Supabase REST API
//  Wiring  : DHT11 VCC→3.3V | DATA→GPIO4 | GND→GND
//            (add 10kΩ pull-up between DATA and VCC)
// ============================================================

#include <WiFi.h>
#include <WebServer.h>
#include <DHT.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>

// ─── WiFi ────────────────────────────────────────────────────
const char* WIFI_SSID     = "POCO X6 Pro 5G";
const char* WIFI_PASSWORD = "pocox6pro5g";

const char* SB_URL   = "https://zdowwogjnrqvzaztpqet.supabase.co";
const char* SB_KEY   = "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiJzdXBhYmFzZSIsInJlZiI6Inpkb3d3b2dqbnJxdnphenRwcWV0Iiwicm9sZSI6ImFub24iLCJpYXQiOjE3NzU5MzI2NTEsImV4cCI6MjA5MTUwODY1MX0.2IrDSUflnZZm3YkAKtVq-JdOhM-Yjui_2iWr0qWKE5s";
const char* SB_TABLE = "sensor_readings";

// ─── DHT11 ───────────────────────────────────────────────────
#define DHTPIN  4
#define DHTTYPE DHT11

// ─── Globals ─────────────────────────────────────────────────
WebServer server(80);
DHT       dht(DHTPIN, DHTTYPE);

float temperature  = 0;
float humidity     = 0;
float uvIndex      = 0;
float windSpeed    = 0;
bool  sensorOK     = false;
bool  rainForecast = false;

unsigned long lastReadTime = 0;
unsigned long lastSendTime = 0;

const unsigned long READ_INTERVAL = 10000UL;   // 10 s
const unsigned long SEND_INTERVAL = 30000UL;   // 30 s

// ─── Supabase POST ───────────────────────────────────────────
void sendToSupabase() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[Supabase] WiFi not connected - skip");
    return;
  }

  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient http;
  String url = String(SB_URL) + "/rest/v1/" + SB_TABLE;
  http.begin(client, url);
  http.addHeader("Content-Type",  "application/json");
  http.addHeader("apikey",        SB_KEY);
  http.addHeader("Authorization", String("Bearer ") + SB_KEY);
  http.addHeader("Prefer",        "return=minimal");

  String body =
    "{\"temperature\":"   + String(temperature,  1) +
    ",\"humidity\":"      + String(humidity,     1) +
    ",\"sensor_ok\":"     + (sensorOK     ? "true" : "false") +
    ",\"uv_index\":"      + String(uvIndex,      1) +
    ",\"wind_speed\":"    + String(windSpeed,    1) +
    ",\"rain_forecast\":" + (rainForecast  ? "true" : "false") +
    "}";

  int code = http.POST(body);
  if (code > 0) Serial.printf("[Supabase] HTTP %d\n", code);
  else Serial.printf("[Supabase] Error: %s\n", http.errorToString(code).c_str());
  http.end();
}

// ─── Web Handlers ────────────────────────────────────────────
void handleRoot() {
  String html = "<!DOCTYPE html><html><head>"
    "<meta charset='UTF-8'>"
    "<meta name='viewport' content='width=device-width,initial-scale=1'>"
    "<title>ESP32 Weather</title>"
    "<style>"
      "body{margin:0;font-family:monospace;background:#0d1117;color:#58a6ff;"
           "display:flex;flex-direction:column;align-items:center;justify-content:center;min-height:100vh}"
      "h2{margin:0 0 24px;font-size:1.1rem;color:#8b949e;letter-spacing:.1em}"
      ".grid{display:grid;grid-template-columns:1fr 1fr;gap:12px;width:280px}"
      ".c{background:#161b22;border:1px solid #30363d;border-radius:8px;padding:14px;text-align:center}"
      ".v{font-size:1.6rem;margin-bottom:4px}"
      ".l{font-size:.7rem;color:#8b949e;text-transform:uppercase;letter-spacing:.08em}"
      ".ok{color:#3fb950}.err{color:#f85149}.rain-y{color:#58a6ff}.rain-n{color:#8b949e}"
    "</style></head><body>"
    "<h2>ESP32 WEATHER STATION</h2>"
    "<div class='grid'>"
    "<div class='c'><div class='v' style='color:#ff7b72'>" + String(temperature,1) + "&deg;C</div><div class='l'>Temperature</div></div>"
    "<div class='c'><div class='v' style='color:#58a6ff'>" + String(humidity,   1) + "%</div><div class='l'>Humidity</div></div>"
    "<div class='c'><div class='v' style='color:#d2a8ff'>" + String(uvIndex,    1) + "</div><div class='l'>UV Index</div></div>"
    "<div class='c'><div class='v' style='color:#3fb950'>" + String(windSpeed,  1) + " m/s</div><div class='l'>Wind</div></div>"
    "<div class='c'><div class='v " + String(rainForecast?"rain-y":"rain-n") + "'>" + (rainForecast?"YES":"NO") + "</div><div class='l'>Rain</div></div>"
    "<div class='c'><div class='v " + String(sensorOK?"ok":"err") + "'>"           + (sensorOK?"OK":"ERR")     + "</div><div class='l'>Sensor</div></div>"
    "</div></body></html>";
  server.send(200, "text/html", html);
}

void handleData() {
  String json =
    "{\"temperature\":"   + String(temperature,  1) +
    ",\"humidity\":"      + String(humidity,     1) +
    ",\"uv_index\":"      + String(uvIndex,      1) +
    ",\"wind_speed\":"    + String(windSpeed,    1) +
    ",\"rain_forecast\":" + (rainForecast ? "true" : "false") +
    ",\"sensor_ok\":"     + (sensorOK    ? "true" : "false") +
    "}";
  server.send(200, "application/json", json);
}

void handleNotFound() { server.send(404, "text/plain", "Not found"); }

// ─── Setup ───────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  dht.begin();
  randomSeed(analogRead(0));

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("[WiFi] Connecting");
  while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print("."); }
  Serial.println();
  Serial.print("[WiFi] Connected! IP: ");
  Serial.println(WiFi.localIP());

  server.on("/",     handleRoot);
  server.on("/data", handleData);
  server.onNotFound(handleNotFound);
  server.begin();
  Serial.println("[HTTP] Server started");
}

// ─── Loop ────────────────────────────────────────────────────
void loop() {
  server.handleClient();
  unsigned long now = millis();

  if (now - lastReadTime >= READ_INTERVAL) {
    lastReadTime = now;
    float t = dht.readTemperature();
    float h = dht.readHumidity();
    if (!isnan(t) && !isnan(h)) { temperature = t; humidity = h; sensorOK = true; }
    else { sensorOK = false; Serial.println("[DHT11] Read failed!"); }

    uvIndex      = random(0, 110) / 10.0;
    windSpeed    = random(0, 150) / 10.0;
    rainForecast = random(0, 2);

    Serial.printf("[Sensor] T:%.1f C  H:%.1f%%  UV:%.1f  Wind:%.1f m/s  Rain:%s\n",
                  temperature, humidity, uvIndex, windSpeed, rainForecast?"YES":"NO");
  }

  if (now - lastSendTime >= SEND_INTERVAL) {
    lastSendTime = now;
    sendToSupabase();
  }
}
