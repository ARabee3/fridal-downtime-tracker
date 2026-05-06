#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include "../include/config.h"
#include <esp_system.h>

static bool wifiReady = false;

static String buildUrl(const char *endpoint) {
  return String("http://") + SERVER_HOST + ":" + String(SERVER_PORT) + SERVER_API_BASE + "/" + endpoint;
}

static bool sendPost(const char *endpoint) {
  if (!wifiReady) {
    PRINT_DEBUG("WiFi is not connected\n");
    return false;
  }

  String url = buildUrl(endpoint);
  String payload = String("{\"machine_id\":\"") + MACHINE_ID + "\"}";

  HTTPClient http;
  http.begin(url);
  http.addHeader("Content-Type", "application/json");

  PRINT_DEBUG("POST %s -> %s\n", endpoint, url.c_str());
  int code = http.POST(payload);
  String response = http.getString();
  http.end();

  PRINT_DEBUG("HTTP code: %d\n", code);
  if (response.length()) {
    PRINT_DEBUG("Response: %s\n", response.c_str());
  }

  return code >= 200 && code < 300;
}

static void connectWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  PRINT_DEBUG("Connecting to WiFi: %s\n", WIFI_SSID);
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 15000UL) {
    delay(500);
    PRINT_DEBUG(".");
  }
  PRINT_DEBUG("\n");

  wifiReady = (WiFi.status() == WL_CONNECTED);
  if (wifiReady) {
    PRINT_DEBUG("WiFi connected, IP: %s\n", WiFi.localIP().toString().c_str());
  } else {
    PRINT_DEBUG("WiFi connect failed\n");
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  // Print local efuse MAC (works without connecting to WiFi)
  uint8_t efuse_mac[6];
  if (esp_read_mac(efuse_mac, ESP_MAC_WIFI_STA) == ESP_OK) {
    char macbuf[18];
    snprintf(macbuf, sizeof(macbuf), "%02X:%02X:%02X:%02X:%02X:%02X",
             efuse_mac[0], efuse_mac[1], efuse_mac[2], efuse_mac[3], efuse_mac[4], efuse_mac[5]);
    Serial.printf("Local MAC (efuse): %s\n", macbuf);
  } else {
    Serial.println("Local MAC: <error>");
  }

  // Print configured secondary peer MAC from config for reference (if present)
#ifdef SECONDARY_PEER_MAC
  const uint8_t cfg_mac[6] = SECONDARY_PEER_MAC;
  Serial.printf("Configured SECONDARY_PEER_MAC: %02X:%02X:%02X:%02X:%02X:%02X\n",
                cfg_mac[0], cfg_mac[1], cfg_mac[2], cfg_mac[3], cfg_mac[4], cfg_mac[5]);
#else
  Serial.println("Configured SECONDARY_PEER_MAC: <not defined>");
#endif


  PRINT_DEBUG("\n=== Simple Endpoint Test ===\n");
  connectWiFi();
  if (!wifiReady) {
    return;
  }

  PRINT_DEBUG("Sending failure-start...\n");
  sendPost("failure-start");

  PRINT_DEBUG("Waiting 50 seconds...\n");
  delay(50000UL);

  PRINT_DEBUG("Sending failure-end...\n");
  sendPost("failure-end");

  PRINT_DEBUG("Test complete.\n");
}

void loop() {
  delay(1000);
}
