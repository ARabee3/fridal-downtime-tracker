#include "ServerComms.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include "../include/config.h"

ServerComms::ServerComms() {}

// connectWiFi(): establish WiFi connection
bool ServerComms::connectWiFi(const char *ssid, const char *password) {
  #if BOARD_DEBUG
  Serial.printf("Connecting to WiFi: %s\n", ssid);
  #endif

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  int attempts = 0;
  const int maxAttempts = 20; // ~10 seconds with 500ms delay
  while (WiFi.status() != WL_CONNECTED && attempts < maxAttempts) {
    delay(500);
    #if BOARD_DEBUG
    Serial.print(".");
    #endif
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    #if BOARD_DEBUG
    Serial.printf("\nWiFi connected! IP: %s\n", WiFi.localIP().toString().c_str());
    #endif
    return true;
  } else {
    #if BOARD_DEBUG
    Serial.println("\nFailed to connect to WiFi");
    #endif
    return false;
  }
}

bool ServerComms::isWiFiConnected() const {
  return WiFi.status() == WL_CONNECTED;
}

// _sendRequest(): generic HTTP POST helper
bool ServerComms::_sendRequest(const char *endpoint, const char *payload) {
  if (!isWiFiConnected()) {
    #if BOARD_DEBUG
    Serial.println("WiFi not connected, cannot send request");
    #endif
    return false;
  }

  // Build the full URL
  char url[256];
  snprintf(url, sizeof(url), "http://%s:%d%s/%s", SERVER_HOST, SERVER_PORT, SERVER_API_PATH, endpoint);

  HTTPClient http;
  http.begin(url);
  http.addHeader("Content-Type", "application/json");

  int httpCode = http.POST((uint8_t *)payload, strlen(payload));
  bool success = (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_CREATED);

  #if BOARD_DEBUG
  Serial.printf("[ServerComms] POST %s -> HTTP %d\n", endpoint, httpCode);
  if (!success) {
    Serial.printf("Response: %s\n", http.getString().c_str());
  }
  #endif

  http.end();
  return success;
}

// sendFailureStart(): report when a failure begins
bool ServerComms::sendFailureStart(const char *machineId, const char *timestamp) {
  char payload[256];
  if (timestamp) {
    snprintf(payload, sizeof(payload), "{\"machine_id\":\"%s\",\"timestamp\":\"%s\"}", machineId, timestamp);
  } else {
    snprintf(payload, sizeof(payload), "{\"machine_id\":\"%s\"}", machineId);
  }

  #if BOARD_DEBUG
  Serial.printf("[ServerComms] Sending failure START: %s\n", payload);
  #endif

  return _sendRequest("failure-start", payload);
}

// sendFailureStop(): report when a failure ends
bool ServerComms::sendFailureStop(const char *machineId) {
  char payload[256];
  snprintf(payload, sizeof(payload), "{\"machine_id\":\"%s\"}", machineId);

  #if BOARD_DEBUG
  Serial.printf("[ServerComms] Sending failure STOP: %s\n", payload);
  #endif

  return _sendRequest("failure-stop", payload);
}
