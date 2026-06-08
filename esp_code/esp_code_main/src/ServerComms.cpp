#include "../include/ServerComms.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include "../include/config.h"

static const char* wifiStatusName(wl_status_t status) {
  switch (status) {
    case WL_NO_SHIELD:       return "NO_SHIELD";
    case WL_IDLE_STATUS:     return "IDLE";
    case WL_NO_SSID_AVAIL:   return "NO_SSID_AVAIL";
    case WL_SCAN_COMPLETED:  return "SCAN_COMPLETED";
    case WL_CONNECTED:       return "CONNECTED";
    case WL_CONNECT_FAILED:  return "CONNECT_FAILED";
    case WL_CONNECTION_LOST: return "CONNECTION_LOST";
    case WL_DISCONNECTED:    return "DISCONNECTED";
    default:                 return "UNKNOWN";
  }
}

static void scanAndPrintNetworks(const char *targetSsid) {
  PRINT_DEBUG("[WiFi] Scanning networks...\n");
  int n = WiFi.scanNetworks();
  if (n <= 0) {
    PRINT_DEBUG("[WiFi] No networks found! Check antenna / board.\n");
    return;
  }
  PRINT_DEBUG("[WiFi] Found %d network(s):\n", n);
  bool foundTarget = false;
  for (int i = 0; i < n; i++) {
    const char *marker = (WiFi.SSID(i) == targetSsid) ? " <-- TARGET" : "";
    PRINT_DEBUG("  %2d: %-32s ch=%2d rssi=%d dBm%s\n",
                i + 1, WiFi.SSID(i).c_str(), WiFi.channel(i),
                WiFi.RSSI(i), marker);
    if (WiFi.SSID(i) == targetSsid) foundTarget = true;
  }
  if (!foundTarget) {
    PRINT_DEBUG("[WiFi] WARNING: Target SSID \"%s\" NOT FOUND in scan!\n", targetSsid);
    PRINT_DEBUG("[WiFi] Check: is the AP on? is it 2.4GHz? is SSID hidden?\n");
  }
  WiFi.scanDelete();
}

ServerComms::ServerComms() {
  _lastStopId[0] = '\0';
  _wifiInitDone = false;
}

bool ServerComms::connectWiFi(const char *ssid, const char *password) {
  PRINT_DEBUG("[WiFi] Connecting to: %s\n", ssid);

  if (WiFi.status() != WL_CONNECTED) {
    WiFi.mode(WIFI_STA);
    _wifiInitDone = true;

    scanAndPrintNetworks(ssid);

    WiFi.begin(ssid, password);
  } else {
    PRINT_DEBUG("[WiFi] Already connected\n");
    return true;
  }

  int attempts = 0;
  const int maxAttempts = 40;
  wl_status_t lastStatus = WiFi.status();
  while (WiFi.status() != WL_CONNECTED && attempts < maxAttempts) {
    delay(500);
    wl_status_t current = WiFi.status();
    if (current != lastStatus) {
      PRINT_DEBUG("\n[WiFi] Status changed: %s -> %s (code %d)\n",
                  wifiStatusName(lastStatus), wifiStatusName(current), (int)current);
      lastStatus = current;
    } else {
      PRINT_DEBUG(".");
    }
    attempts++;
  }

  wl_status_t final = WiFi.status();
  if (final == WL_CONNECTED) {
    PRINT_DEBUG("\n[WiFi] Connected! IP: %s, Channel: %d, RSSI: %d dBm\n",
                WiFi.localIP().toString().c_str(), WiFi.channel(), WiFi.RSSI());
    return true;
  }

  PRINT_DEBUG("\n[WiFi] Failed. Final status: %s (code %d)\n",
              wifiStatusName(final), (int)final);
  if (final == WL_CONNECT_FAILED) {
    PRINT_DEBUG("[WiFi] CONNECT_FAILED usually means wrong password.\n");
  } else if (final == WL_NO_SSID_AVAIL) {
    PRINT_DEBUG("[WiFi] NO_SSID_AVAIL means AP not seen — is it 2.4GHz?\n");
  }
  return false;
}

void ServerComms::reconnectWiFi(const char *ssid, const char *password) {
  if (WiFi.status() == WL_CONNECTED) return;

  if (!_wifiInitDone) {
    WiFi.mode(WIFI_STA);
    _wifiInitDone = true;
  }

  PRINT_DEBUG("[WiFi] Status: %s (code %d)\n",
              wifiStatusName(WiFi.status()), (int)WiFi.status());
  WiFi.begin(ssid, password);
  PRINT_DEBUG("[WiFi] Reconnect started (async)\n");
}

bool ServerComms::isWiFiConnected() const {
  return WiFi.status() == WL_CONNECTED;
}

bool ServerComms::_sendRequest(const char *endpoint, const char *payload,
                                char *outStopId, size_t outLen) {
  if (!isWiFiConnected()) {
    PRINT_DEBUG("[HTTP] WiFi not connected, cannot send\n");
    return false;
  }

  char url[256];
  snprintf(url, sizeof(url), "http://%s:%d%s/%s",
           SERVER_HOST, SERVER_PORT, SERVER_API_PATH, endpoint);

  HTTPClient http;
  http.begin(url);
  http.addHeader("Content-Type", "application/json");
  http.setTimeout(HTTP_TIMEOUT_MS);

  int httpCode = http.POST((uint8_t *)payload, strlen(payload));

  PRINT_DEBUG("[HTTP] POST %s -> %d\n", endpoint, httpCode);

  bool success = (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_CREATED);

  if (success && outStopId && outLen > 0) {
    String response = http.getString();
    const char *idPtr = strstr(response.c_str(), "\"stop_id\":\"");
    if (idPtr) {
      idPtr += 11;
      const char *idEnd = strchr(idPtr, '"');
      if (idEnd) {
        size_t idLen = idEnd - idPtr;
        if (idLen < outLen) {
          memcpy(outStopId, idPtr, idLen);
          outStopId[idLen] = '\0';
          PRINT_DEBUG("[HTTP] Saved stop_id: %s\n", outStopId);
        }
      }
    }
  }

  if (!success) {
    String body = http.getString();
    PRINT_DEBUG("[HTTP] Response: %s\n", body.c_str());
  }

  http.end();
  return success;
}

bool ServerComms::sendFailureStart(const char *machineId, const char *timestamp) {
  char payload[256];
  if (timestamp) {
    snprintf(payload, sizeof(payload),
             "{\"machine_id\":\"%s\",\"timestamp\":\"%s\"}", machineId, timestamp);
  } else {
    snprintf(payload, sizeof(payload), "{\"machine_id\":\"%s\"}", machineId);
  }

  PRINT_DEBUG("[HTTP] Sending failure-start: %s\n", payload);

  char stopId[64] = "";
  bool ok = _sendRequest("failure-start", payload, stopId, sizeof(stopId));
  if (ok && stopId[0]) {
    strncpy(_lastStopId, stopId, sizeof(_lastStopId) - 1);
    _lastStopId[sizeof(_lastStopId) - 1] = '\0';
  }
  return ok;
}

bool ServerComms::sendFailureStop(const char *machineId) {
  char payload[256];
  if (_lastStopId[0]) {
    snprintf(payload, sizeof(payload),
             "{\"machine_id\":\"%s\",\"stop_id\":\"%s\"}", machineId, _lastStopId);
  } else {
    snprintf(payload, sizeof(payload), "{\"machine_id\":\"%s\"}", machineId);
  }

  PRINT_DEBUG("[HTTP] Sending failure-stop: %s\n", payload);

  bool ok = _sendRequest("failure-end", payload);
  if (ok) {
    _lastStopId[0] = '\0';
  }
  return ok;
}
