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

// ─── Shared connection helper ───────────────────────────────────────────────
// This is the core connect routine used by both connectWiFi and reconnectWiFi.
// It does a scan, targets the best AP by BSSID+channel, disables power save,
// and waits for connection with detailed status logging.
//
// KEY FIX: We use WiFi.begin() instead of raw esp_wifi_set_config()+esp_wifi_connect().
// The raw API does NOT properly reset the radio state machine after a scan,
// which causes AUTH_EXPIRE (reason 2) — the 802.11 auth frame exchange times
// out because the radio is stuck in the wrong internal state.

static bool doConnect(const char *ssid, const char *password,
                      bool doScan, int maxWaitSec) {
  // Reset last disconnect reason for this attempt
  s_lastDisconnectReason = 0;

  int targetChannel = 0;
  uint8_t targetBssid[6] = {0};
  wifi_auth_mode_t targetAuth = WIFI_AUTH_OPEN;
  bool haveBssid = false;

  if (doScan) {
    haveBssid = scanAndFindTarget(ssid, targetChannel, targetBssid, targetAuth);

    if (haveBssid) {
      if (targetAuth == WIFI_AUTH_WPA2_ENTERPRISE) {
        PRINT_DEBUG("[WiFi] AP uses WPA2-ENTERPRISE — ESP cannot connect with PSK.\n");
        return false;
      }
      if (targetChannel > 14) {
        PRINT_DEBUG("[WiFi] Channel %d > 14 — 5GHz not supported by ESP32-S3.\n", targetChannel);
        return false;
      }
    }
  }

  // ── Full STA teardown + re-init ──
  // This is critical: WiFi.disconnect(true) tears down the STA interface
  // completely, then WiFi.mode(WIFI_STA) re-initializes it cleanly.
  // Without this, the radio can be stuck in a broken state after scanning.
  WiFi.disconnect(true);   // disconnect + turn off STA
  delay(100);
  WiFi.mode(WIFI_STA);     // re-enable STA from scratch
  delay(100);

  // ── Disable long-range mode — use standard b/g/n ──
  esp_wifi_set_protocol(WIFI_IF_STA, WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G | WIFI_PROTOCOL_11N);

  // ── Disable power save (modem sleep) ──
  esp_wifi_set_ps(WIFI_PS_NONE);

  // ── Connect using Arduino WiFi.begin() ──
  // WiFi.begin() properly handles the internal state machine, unlike raw
  // esp_wifi_set_config() + esp_wifi_connect() which skips critical steps.
  if (haveBssid && targetChannel > 0) {
    PRINT_DEBUG("[WiFi] Targeting BSSID %02X:%02X:%02X:%02X:%02X:%02X on ch %d\n",
                targetBssid[0], targetBssid[1], targetBssid[2],
                targetBssid[3], targetBssid[4], targetBssid[5], targetChannel);
    WiFi.begin(ssid, password, targetChannel, targetBssid, true);
  } else {
    PRINT_DEBUG("[WiFi] Connecting without BSSID lock\n");
    WiFi.begin(ssid, password);
  }

  // Wait for connection with status monitoring
  int maxAttempts = maxWaitSec * 2;  // 500ms per iteration
  wl_status_t lastStatus = WL_IDLE_STATUS;

  for (int i = 0; i < maxAttempts; i++) {
    delay(500);
    wl_status_t current = WiFi.status();

    if (current == WL_CONNECTED) {
      PRINT_DEBUG("\n[WiFi] Connected! IP: %s  Channel: %d  RSSI: %d dBm\n",
                  WiFi.localIP().toString().c_str(), WiFi.channel(), WiFi.RSSI());
      // Confirm power save is off
      esp_wifi_set_ps(WIFI_PS_NONE);
      return true;
    }

    if (current != lastStatus) {
      PRINT_DEBUG("\n[WiFi] Status: %s (code %d)", wifiStatusName(current), (int)current);
      if (s_lastDisconnectReason != 0) {
        PRINT_DEBUG(" | Last disconnect reason: %d (%s)",
                    s_lastDisconnectReason, disconnectReasonName(s_lastDisconnectReason));
      }
      PRINT_DEBUG("\n");
      lastStatus = current;

      // If the AP explicitly rejected us, don't keep waiting
      if (current == WL_CONNECT_FAILED) {
        PRINT_DEBUG("[WiFi] Auth rejected by AP. Aborting wait.\n");
        break;
      }
    } else {
      PRINT_DEBUG(".");
    }
  }

  PRINT_DEBUG("\n[WiFi] Failed after %ds. Final: %s (code %d)\n",
              maxWaitSec, wifiStatusName(WiFi.status()), (int)WiFi.status());

  if (s_lastDisconnectReason != 0) {
    PRINT_DEBUG("[WiFi] ESP-IDF disconnect reason: %d (%s)\n",
                s_lastDisconnectReason, disconnectReasonName(s_lastDisconnectReason));

    if (s_lastDisconnectReason == 2 || s_lastDisconnectReason == 202) {
      PRINT_DEBUG("[WiFi] >>> AUTH issue: Wrong password, MAC filter, or WPA3 mismatch.\n");
      PRINT_DEBUG("[WiFi] >>> Try setting router to WPA2-PSK only (no WPA3).\n");
    } else if (s_lastDisconnectReason == 14 || s_lastDisconnectReason == 204) {
      PRINT_DEBUG("[WiFi] >>> HANDSHAKE_TIMEOUT: AP too slow or signal too weak.\n");
    } else if (s_lastDisconnectReason == 201) {
      PRINT_DEBUG("[WiFi] >>> NO_AP_FOUND: AP not visible. Is it on? 2.4GHz? Hidden?\n");
    } else if (s_lastDisconnectReason == 15) {
      PRINT_DEBUG("[WiFi] >>> GROUP_KEY_TIMEOUT: WPA3/PMF issue. Try WPA2-only on router.\n");
    }
  }

  return false;
}

// ─── Class implementation ───────────────────────────────────────────────────

ServerComms::ServerComms() {
  _lastStopId[0] = '\0';
  _wifiInitDone = false;
  _reconnectCount = 0;
}

bool ServerComms::connectWiFi(const char *ssid, const char *password) {
  PRINT_DEBUG("[WiFi] Connecting to: %s\n", ssid);

  if (WiFi.status() == WL_CONNECTED) {
    PRINT_DEBUG("[WiFi] Already connected\n");
    return true;
  }

  WiFi.mode(WIFI_STA);
  _wifiInitDone = true;

  // Install event handler to capture real disconnect reasons
  installWiFiEventHandler();

  // Disable auto-reconnect — we handle it ourselves in the main loop
  WiFi.setAutoReconnect(false);

  // Attempt 1: scan + targeted BSSID connect (20s timeout)
  PRINT_DEBUG("[WiFi] === Attempt 1: Targeted connect ===\n");
  if (doConnect(ssid, password, true, 20)) {
    return true;
  }

  // Attempt 2: simple connect without BSSID lock (15s timeout)
  PRINT_DEBUG("[WiFi] === Attempt 2: Fallback (no BSSID lock) ===\n");
  if (doConnect(ssid, password, false, 15)) {
    return true;
  }

  PRINT_DEBUG("[WiFi] Both attempts failed.\n");
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

  PRINT_DEBUG("[WiFi] Reconnect attempt #%d | Status: %s (code %d)\n",
              _reconnectCount, wifiStatusName(WiFi.status()), (int)WiFi.status());

  // Every 3rd attempt, do a full scan to re-target the AP (it may have
  // changed channel). Other attempts use a fast non-scan connect.
  bool doScan = (_reconnectCount % 3 == 1);

  if (doScan) {
    PRINT_DEBUG("[WiFi] Reconnect with scan (re-targeting AP)\n");
  }

  // Use a shorter timeout for reconnect so loop() isn't blocked too long
  doConnect(ssid, password, doScan, 10);
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