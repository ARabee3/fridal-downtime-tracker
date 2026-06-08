#include "../include/ServerComms.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <esp_wifi.h>
#include "../include/config.h"

// ─── Disconnect reason logging ──────────────────────────────────────────────
// Stores the last ESP-IDF disconnect reason so we can print the REAL cause
static uint8_t s_lastDisconnectReason = 0;
static bool s_eventHandlerInstalled = false;

static const char* disconnectReasonName(uint8_t reason) {
  switch (reason) {
    case 1:  return "UNSPECIFIED";
    case 2:  return "AUTH_EXPIRE";
    case 3:  return "AUTH_LEAVE";
    case 4:  return "ASSOC_EXPIRE";
    case 5:  return "ASSOC_TOOMANY";
    case 6:  return "NOT_AUTHED";
    case 7:  return "NOT_ASSOCED";
    case 8:  return "ASSOC_LEAVE";
    case 9:  return "ASSOC_NOT_AUTHED";
    case 10: return "DISASSOC_PWRCAP_BAD";
    case 11: return "DISASSOC_SUPCHAN_BAD";
    case 12: return "IE_INVALID";
    case 13: return "MIC_FAILURE";
    case 14: return "4WAY_HANDSHAKE_TIMEOUT";
    case 15: return "GROUP_KEY_UPDATE_TIMEOUT";
    case 16: return "IE_IN_4WAY_DIFFERS";
    case 17: return "GROUP_CIPHER_INVALID";
    case 18: return "PAIRWISE_CIPHER_INVALID";
    case 19: return "AKMP_INVALID";
    case 200: return "BEACON_TIMEOUT";
    case 201: return "NO_AP_FOUND";
    case 202: return "AUTH_FAIL";
    case 203: return "ASSOC_FAIL";
    case 204: return "HANDSHAKE_TIMEOUT";
    case 205: return "CONNECTION_FAIL";
    case 206: return "AP_TSF_RESET";
    default: return "UNKNOWN";
  }
}

static void installWiFiEventHandler() {
  if (s_eventHandlerInstalled) return;
  s_eventHandlerInstalled = true;

  WiFi.onEvent([](WiFiEvent_t event, WiFiEventInfo_t info) {
    uint8_t reason = info.wifi_sta_disconnected.reason;
    s_lastDisconnectReason = reason;
    PRINT_DEBUG("[WiFi-EVENT] DISCONNECTED — reason %d (%s)\n",
                reason, disconnectReasonName(reason));
  }, ARDUINO_EVENT_WIFI_STA_DISCONNECTED);

  WiFi.onEvent([](WiFiEvent_t event, WiFiEventInfo_t info) {
    PRINT_DEBUG("[WiFi-EVENT] GOT IP: %s\n",
                IPAddress(info.got_ip.ip_info.ip.addr).toString().c_str());
  }, ARDUINO_EVENT_WIFI_STA_GOT_IP);
}

// ─── Helper name lookups ────────────────────────────────────────────────────

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

static const char* wifiAuthName(wifi_auth_mode_t mode) {
  switch (mode) {
    case WIFI_AUTH_OPEN:            return "OPEN";
    case WIFI_AUTH_WEP:             return "WEP";
    case WIFI_AUTH_WPA_PSK:         return "WPA-PSK";
    case WIFI_AUTH_WPA2_PSK:        return "WPA2-PSK";
    case WIFI_AUTH_WPA_WPA2_PSK:    return "WPA/WPA2-PSK";
    case WIFI_AUTH_WPA2_ENTERPRISE: return "WPA2-ENTERPRISE";
    case WIFI_AUTH_WPA3_PSK:        return "WPA3-PSK";
    case WIFI_AUTH_WPA2_WPA3_PSK:   return "WPA2/WPA3-PSK";
    case WIFI_AUTH_WAPI_PSK:        return "WAPI-PSK";
    default:                        return "UNKNOWN";
  }
}

// ─── AP scanner ─────────────────────────────────────────────────────────────

static bool scanAndFindTarget(const char *targetSsid,
                              int &outChannel, uint8_t *outBssid,
                              wifi_auth_mode_t &outAuth) {
  PRINT_DEBUG("[WiFi] Scanning for target AP...\n");
  int n = WiFi.scanNetworks(false, false, false, 300);  // active scan, 300ms per channel
  if (n <= 0) {
    PRINT_DEBUG("[WiFi] No networks found! Check antenna / board.\n");
    WiFi.scanDelete();
    return false;
  }
  PRINT_DEBUG("[WiFi] Found %d network(s):\n", n);

  int bestIdx = -1;
  int bestRssi = -999;

  for (int i = 0; i < n; i++) {
    String found = WiFi.SSID(i);
    wifi_auth_mode_t auth = WiFi.encryptionType(i);
    int ch = WiFi.channel(i);
    int rssi = WiFi.RSSI(i);
    const char *marker = (found == targetSsid) ? " <-- TARGET" : "";

    PRINT_DEBUG("  %2d: %-32s ch=%2d rssi=%d dBm  %s%s\n",
                i + 1, found.c_str(), ch, rssi, wifiAuthName(auth), marker);

    if (found == targetSsid && rssi > bestRssi) {
      bestIdx = i;
      bestRssi = rssi;
    }
  }

  if (bestIdx >= 0) {
    outChannel = WiFi.channel(bestIdx);
    outAuth = WiFi.encryptionType(bestIdx);
    uint8_t *bssid = WiFi.BSSID(bestIdx);
    if (bssid) {
      memcpy(outBssid, bssid, 6);
    }
    PRINT_DEBUG("[WiFi] Best AP: ch=%d rssi=%d dBm BSSID=%02X:%02X:%02X:%02X:%02X:%02X\n",
                outChannel, bestRssi,
                outBssid[0], outBssid[1], outBssid[2],
                outBssid[3], outBssid[4], outBssid[5]);
    WiFi.scanDelete();
    return true;
  }

  PRINT_DEBUG("[WiFi] WARNING: Target SSID \"%s\" NOT FOUND!\n", targetSsid);
  PRINT_DEBUG("[WiFi] Check: is AP on? is it 2.4GHz? hidden?\n");
  WiFi.scanDelete();
  return false;
}

// ─── Shared wait-for-connection helper ──────────────────────────────────────

static bool waitForConnect(int maxWaitSec) {
  int maxAttempts = maxWaitSec * 2;  // 500ms per iteration
  wl_status_t lastStatus = WL_IDLE_STATUS;

  for (int i = 0; i < maxAttempts; i++) {
    delay(500);
    wl_status_t current = WiFi.status();

    if (current == WL_CONNECTED) {
      PRINT_DEBUG("\n[WiFi] Connected! IP: %s  Channel: %d  RSSI: %d dBm\n",
                  WiFi.localIP().toString().c_str(), WiFi.channel(), WiFi.RSSI());
      esp_wifi_set_ps(WIFI_PS_NONE);
      return true;
    }

    if (current != lastStatus) {
      PRINT_DEBUG("\n[WiFi] Status: %s (code %d)", wifiStatusName(current), (int)current);
      if (s_lastDisconnectReason != 0) {
        PRINT_DEBUG(" | reason: %d (%s)",
                    s_lastDisconnectReason, disconnectReasonName(s_lastDisconnectReason));
      }
      PRINT_DEBUG("\n");
      lastStatus = current;

      if (current == WL_CONNECT_FAILED) {
        PRINT_DEBUG("[WiFi] Auth rejected by AP. Aborting.\n");
        break;
      }
    } else {
      PRINT_DEBUG(".");
    }
  }

  PRINT_DEBUG("\n[WiFi] Failed after %ds. Final: %s (code %d)\n",
              maxWaitSec, wifiStatusName(WiFi.status()), (int)WiFi.status());

  if (s_lastDisconnectReason != 0) {
    PRINT_DEBUG("[WiFi] ESP-IDF reason: %d (%s)\n",
                s_lastDisconnectReason, disconnectReasonName(s_lastDisconnectReason));
  }

  return false;
}

// ─── Reset radio to a clean state ───────────────────────────────────────────

static void resetRadio() {
  WiFi.disconnect(true);   // full STA teardown
  delay(200);
  WiFi.mode(WIFI_STA);     // re-init STA from scratch
  delay(200);
}

// ─── Class implementation ───────────────────────────────────────────────────

ServerComms::ServerComms() {
  _lastStopId[0] = '\0';
  _wifiInitDone = false;
  _reconnectCount = 0;
}

bool ServerComms::connectWiFi(const char *ssid, const char *password) {
  PRINT_DEBUG("[WiFi] Connecting to: %s\n", ssid);

  // Dump password hex bytes so we can verify the compiler didn't mangle
  // special characters like # * $ @
  PRINT_DEBUG("[WiFi] Password hex: ");
  for (int i = 0; password[i]; i++) {
    PRINT_DEBUG("%02X ", (uint8_t)password[i]);
  }
  PRINT_DEBUG("  (%d bytes)\n", (int)strlen(password));

  if (WiFi.status() == WL_CONNECTED) {
    PRINT_DEBUG("[WiFi] Already connected\n");
    return true;
  }

  WiFi.mode(WIFI_STA);
  _wifiInitDone = true;

  installWiFiEventHandler();
  WiFi.setAutoReconnect(false);

  // ═══════════════════════════════════════════════════════════════════
  // ATTEMPT 1: Simplest possible — plain WiFi.begin(), no scan, no
  // BSSID targeting, no protocol changes. Just the absolute minimum.
  // If this works, the issue was in scan/radio-state management.
  // ═══════════════════════════════════════════════════════════════════
  PRINT_DEBUG("\n[WiFi] === Attempt 1: Plain WiFi.begin() (no scan) ===\n");
  s_lastDisconnectReason = 0;

  resetRadio();
  WiFi.begin(ssid, password);

  if (waitForConnect(15)) return true;

  // ═══════════════════════════════════════════════════════════════════
  // ATTEMPT 2: Lower auth threshold to WPA-PSK (accepts WPA, WPA2, WPA3).
  // Some routers advertise WPA2-PSK but internally do WPA/WPA2 mixed.
  // Also explicitly configure PMF as capable-but-not-required.
  // ═══════════════════════════════════════════════════════════════════
  PRINT_DEBUG("\n[WiFi] === Attempt 2: esp_wifi_set_config (WPA threshold, PMF capable) ===\n");
  s_lastDisconnectReason = 0;

  resetRadio();

  // Configure but don't auto-connect yet
  WiFi.begin(ssid, password, 0, NULL, false);

  // Now override the config with relaxed auth settings
  wifi_config_t conf;
  esp_wifi_get_config(WIFI_IF_STA, &conf);

  // Accept WPA or higher (not just WPA2+)
  conf.sta.threshold.authmode = WIFI_AUTH_WPA_PSK;

  // PMF: capable but not required
  conf.sta.pmf_cfg.capable = true;
  conf.sta.pmf_cfg.required = false;

  // Disable fast scan — do full channel scan to find best AP
  conf.sta.scan_method = WIFI_ALL_CHANNEL_SCAN;
  conf.sta.sort_method = WIFI_CONNECT_AP_BY_SIGNAL;

  esp_wifi_set_config(WIFI_IF_STA, &conf);

  // Set protocol and power-save AFTER config, BEFORE connect
  esp_wifi_set_protocol(WIFI_IF_STA, WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G | WIFI_PROTOCOL_11N);
  esp_wifi_set_ps(WIFI_PS_NONE);

  // NOW connect
  esp_wifi_connect();

  if (waitForConnect(15)) return true;

  // ═══════════════════════════════════════════════════════════════════
  // ATTEMPT 3: Scan first to find channel+BSSID, then targeted connect.
  // Uses the explicit config path with found AP details.
  // ═══════════════════════════════════════════════════════════════════
  PRINT_DEBUG("\n[WiFi] === Attempt 3: Scan + targeted BSSID connect ===\n");
  s_lastDisconnectReason = 0;

  resetRadio();

  int targetChannel = 0;
  uint8_t targetBssid[6] = {0};
  wifi_auth_mode_t targetAuth = WIFI_AUTH_OPEN;

  if (scanAndFindTarget(ssid, targetChannel, targetBssid, targetAuth)) {
    resetRadio();

    // Use WiFi.begin with channel+BSSID targeting
    WiFi.begin(ssid, password, targetChannel, targetBssid, false);

    // Get the config and adjust auth settings
    esp_wifi_get_config(WIFI_IF_STA, &conf);

    // Match the exact auth mode the AP reported
    conf.sta.threshold.authmode = (targetAuth == WIFI_AUTH_OPEN)
      ? WIFI_AUTH_OPEN : WIFI_AUTH_WPA_PSK;
    conf.sta.pmf_cfg.capable = true;
    conf.sta.pmf_cfg.required = false;

    esp_wifi_set_config(WIFI_IF_STA, &conf);
    esp_wifi_set_protocol(WIFI_IF_STA, WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G | WIFI_PROTOCOL_11N);
    esp_wifi_set_ps(WIFI_PS_NONE);
    esp_wifi_connect();

    if (waitForConnect(20)) return true;
  }

  PRINT_DEBUG("\n[WiFi] All attempts failed.\n");
  PRINT_DEBUG("[WiFi] TROUBLESHOOTING:\n");
  PRINT_DEBUG("[WiFi]   1. Temporarily change router password to something simple (e.g. Test12345678)\n");
  PRINT_DEBUG("[WiFi]   2. Check if MAC filtering is enabled on router\n");
  PRINT_DEBUG("[WiFi]   3. Set router to WPA2-PSK only (disable WPA3)\n");
  PRINT_DEBUG("[WiFi]   4. Set 2.4GHz channel width to 20MHz\n");
  return false;
}

void ServerComms::reconnectWiFi(const char *ssid, const char *password) {
  if (WiFi.status() == WL_CONNECTED) {
    _reconnectCount = 0;
    return;
  }

  if (!_wifiInitDone) {
    WiFi.mode(WIFI_STA);
    _wifiInitDone = true;
    installWiFiEventHandler();
    WiFi.setAutoReconnect(false);
  }

  _reconnectCount++;
  s_lastDisconnectReason = 0;

  PRINT_DEBUG("[WiFi] Reconnect attempt #%d | Status: %s (code %d)\n",
              _reconnectCount, wifiStatusName(WiFi.status()), (int)WiFi.status());

  resetRadio();

  // Every 3rd attempt, do a scan to re-target the AP.
  // Other attempts use plain WiFi.begin() (faster, less risky).
  if (_reconnectCount % 3 == 0) {
    PRINT_DEBUG("[WiFi] Reconnect with scan\n");
    int ch = 0;
    uint8_t bssid[6] = {0};
    wifi_auth_mode_t auth = WIFI_AUTH_OPEN;
    if (scanAndFindTarget(ssid, ch, bssid, auth)) {
      resetRadio();
      WiFi.begin(ssid, password, ch, bssid, true);
    } else {
      WiFi.begin(ssid, password);
    }
  } else {
    WiFi.begin(ssid, password);
  }

  waitForConnect(10);
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

  PRINT_DEBUG("[HTTP] POST %s -> %s\n", endpoint, url);

  HTTPClient http;
  http.begin(url);
  http.addHeader("Content-Type", "application/json");
  http.setTimeout(HTTP_TIMEOUT_MS);

  int httpCode = http.POST((uint8_t *)payload, strlen(payload));

  PRINT_DEBUG("[HTTP] Response code: %d\n", httpCode);

  bool success = (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_CREATED);

  if (success && outStopId && outLen > 0) {
    String response = http.getString();
    PRINT_DEBUG("[HTTP] Body: %s\n", response.c_str());
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
    PRINT_DEBUG("[HTTP] Error body: %s\n", body.c_str());
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