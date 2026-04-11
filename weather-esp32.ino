#include <WiFi.h>
#include <WebServer.h>
#include <DHT.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>

// ========== CONFIGURATION ==========
const char* ssid = "POCO X6 Pro 5G";
const char* password = "pocox6pro5g";

// Supabase
const char* supabase_url = "https://bbshovtvvnxgwtnxkuws.supabase.co";
const char* supabase_anon_key = "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiJzdXBhYmFzZSIsInJlZiI6ImJic2hvdnR2dm54Z3d0bnhrdXdzIiwicm9sZSI6ImFub24iLCJpYXQiOjE3NzU5MDE4NjEsImV4cCI6MjA5MTQ3Nzg2MX0.1PD2Q_Gm4lOr2OMD0TS9dMy5WCmcFfKQmXmckUiQjsU";
const char* supabase_table = "sensor_readings";

// DHT11
#define DHTPIN 4
#define DHTTYPE DHT11

WebServer server(80);
DHT dht(DHTPIN, DHTTYPE);

// Sensor variables (GLOBAL = FIXED SCOPE ISSUE)
float temperature = 0;
float humidity = 0;
bool sensorOK = false;

float uvIndex = 0;
float windSpeed = 0;
bool rainForecast = false;

// timing
unsigned long lastReadTime = 0;
const unsigned long readInterval = 10000;

// ========== SUPABASE FUNCTION ==========
void sendToSupabase() {

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi not connected");
    return;
  }

  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient http;

  String url = String(supabase_url) + "/rest/v1/" + supabase_table;
  http.begin(client, url);

  http.addHeader("Content-Type", "application/json");
  http.addHeader("apikey", supabase_anon_key);
  http.addHeader("Authorization", String("Bearer ") + supabase_anon_key);
  http.addHeader("Prefer", "return=minimal");

  // JSON payload
  String payload = "{";
  payload += "\"temperature\":" + String(temperature, 1) + ",";
  payload += "\"humidity\":" + String(humidity, 1) + ",";
  payload += "\"sensor_ok\":" + String(sensorOK ? "true" : "false") + ",";
  payload += "\"uv_index\":" + String(uvIndex, 1) + ",";
  payload += "\"wind_speed\":" + String(windSpeed, 1) + ",";
  payload += "\"rain_forecast\":" + String(rainForecast ? "true" : "false");
  payload += "}";

  Serial.println("\nSending JSON:");
  Serial.println(payload);

  int httpCode = http.POST(payload);

  Serial.printf("HTTP Code: %d\n", httpCode);
  Serial.println(http.getString());

  http.end();
}

// ========== WEB DASHBOARD ==========
String getDashboardHTML() {
  return "<html><body><h1>ESP32 Dashboard</h1><p>Sending data to Supabase...</p></body></html>";
}

void handleRoot() {
  server.send(200, "text/html", getDashboardHTML());
}

void handleData() {
  String json = "{";
  json += "\"temperature\":" + String(temperature, 1) + ",";
  json += "\"humidity\":" + String(humidity, 1) + ",";
  json += "\"sensor_ok\":" + String(sensorOK ? "true" : "false") + ",";
  json += "\"uv_index\":" + String(uvIndex, 1) + ",";
  json += "\"wind_speed\":" + String(windSpeed, 1) + ",";
  json += "\"rain_forecast\":" + String(rainForecast ? "true" : "false");
  json += "}";

  server.send(200, "application/json", json);
}

// ========== SETUP ==========
void setup() {
  Serial.begin(115200);
  dht.begin();

  WiFi.begin(ssid, password);
  Serial.print("Connecting WiFi");

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nWiFi connected!");
  Serial.println(WiFi.localIP());

  server.on("/", handleRoot);
  server.on("/data", handleData);
  server.begin();
}

// ========== LOOP ==========
void loop() {
  server.handleClient();

  if (millis() - lastReadTime >= readInterval) {
    lastReadTime = millis();

    float t = dht.readTemperature();
    float h = dht.readHumidity();

    if (!isnan(t) && !isnan(h)) {
      temperature = t;
      humidity = h;
      sensorOK = true;
    } else {
      sensorOK = false;
      Serial.println("DHT11 read failed");
    }

    // simulated sensors
    uvIndex = random(0, 110) / 10.0;
    windSpeed = random(0, 150) / 10.0;
    rainForecast = random(0, 2);

    Serial.printf("T: %.1f H: %.1f UV: %.1f WIND: %.1f RAIN: %s\n",
                  temperature, humidity, uvIndex, windSpeed,
                  rainForecast ? "YES" : "NO");

    sendToSupabase();
  }
}