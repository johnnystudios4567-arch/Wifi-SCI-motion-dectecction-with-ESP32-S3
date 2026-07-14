/*
  ESP32-S3 WiFi Motion Radar
  ---------------------------
  Uses WiFi CSI (Channel State Information) to detect movement in a room
  by measuring how much the WiFi signal's subcarrier amplitudes fluctuate
  compared to a slow-moving baseline. More movement = bigger fluctuations
  = higher score. This is far more sensitive than plain RSSI monitoring.

  Hosts a live web page (self-updating graph) directly from the ESP32.
  The HTML/JS for that page lives in webpage.h (see that file for why).

  REQUIRED LIBRARIES (install via Arduino Library Manager):
    - ESP32Async / ESPAsyncWebServer  (search "ESPAsyncWebServer" by ESP32Async)
    - ESP32Async / AsyncTCP           (search "AsyncTCP" by ESP32Async)
c:\Users\store\Downloads\wifi_motion_sensor\webpage.h
  REQUIRED BOARD PACKAGE:
    - arduino-esp32 core v3.0 or newer (CSI functions are only exposed in
      recent cores). Install via Boards Manager -> "esp32" by Espressif.

  BOARD SETTINGS:
    - Board: "ESP32S3 Dev Module"
    - USB CDC On Boot: "Enabled" (so Serial works over USB)

  FILES NEEDED IN THIS SKETCH FOLDER:
    - wifi_motion_sensor.ino  (this file)
    - webpage.h               (the web page HTML/JS)
    Arduino IDE will show webpage.h as a second tab automatically as long
    as it sits in the same folder as this .ino file.
*/

#include <WiFi.h>
#include <esp_wifi.h>
#include <ESPAsyncWebServer.h>
#include "webpage.h"

// ---------- CONFIG ----------
const char* ssid     = "wifi name";
const char* password = "you password";
const unsigned long pushInterval = 150; // ms between graph updates sent to browser
// -----------------------------

AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

#define MAX_SUBCARRIERS 64
volatile float motionScore = 0;
float baseline[MAX_SUBCARRIERS] = {0};
bool  baselineInit = false;
const float alpha = 0.02f;     // how fast the "resting" baseline adapts (slower = more sensitive to new motion)
float smoothedScore = 0;
unsigned long lastPush = 0;

// ---------- CSI CALLBACK ----------
// Fires every time the radio captures channel state info from an incoming packet.
void wifi_csi_rx_cb(void *ctx, wifi_csi_info_t *info) {
  if (!info || !info->buf) return;

  int subcarriers = info->len / 2;
  if (subcarriers < 1) return;
  if (subcarriers > MAX_SUBCARRIERS) subcarriers = MAX_SUBCARRIERS;

  float diffSum = 0;
  for (int i = 0; i < subcarriers; i++) {
    int8_t I = info->buf[i * 2];
    int8_t Q = info->buf[i * 2 + 1];
    float amp = sqrtf((float)(I * I + Q * Q));

    if (!baselineInit) {
      baseline[i] = amp;
    } else {
      diffSum += fabsf(amp - baseline[i]);
      baseline[i] = baseline[i] * (1.0f - alpha) + amp * alpha; // slow-adapting baseline
    }
  }
  baselineInit = true;
  motionScore = diffSum / subcarriers;
}

void setupCSI() {
  wifi_csi_config_t csi_config = {
    .lltf_en = true,
    .htltf_en = true,
    .stbc_htltf2_en = true,
    .ltf_merge_en = true,
    .channel_filter_en = true,
    .manu_scale = true,
    .shift = false,
  };
  esp_wifi_set_csi_config(&csi_config);
  esp_wifi_set_csi_rx_cb(&wifi_csi_rx_cb, NULL);
  esp_wifi_set_csi(true);
}

// ---------- WEBSOCKET EVENTS ----------
void onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client,
               AwsEventType type, void *arg, uint8_t *data, size_t len) {
  // We only push data from the ESP32 side; no need to handle incoming messages.
}

void setup() {
  Serial.begin(115200);
  delay(500);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(300);
    Serial.print(".");
  }
  Serial.println();
  Serial.print("Connected. IP address: ");
  Serial.println(WiFi.localIP());

  setupCSI();

  ws.onEvent(onWsEvent);
  server.addHandler(&ws);

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send_P(200, "text/html", index_html);
  });

  server.begin();
  Serial.println("Web server started. Open http://<the IP above>/ in a browser.");
}

void loop() {
  ws.cleanupClients();

  unsigned long now = millis();
  if (now - lastPush >= pushInterval) {
    lastPush = now;
    smoothedScore = smoothedScore * 0.7f + motionScore * 0.3f;

    char buf[48];
    snprintf(buf, sizeof(buf), "{\"m\":%.2f}", smoothedScore);
    ws.textAll(buf);
  }
}
