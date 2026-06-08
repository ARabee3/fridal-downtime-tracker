#include <Arduino.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include "../include/config.h"
#include "CurrentSensor.h"
#include "EspNowDriver.h"
#include "ServerComms.h"

static const uint8_t SECONDARY_MAC[6] = SECONDARY_PEER_MAC;

CurrentSensor currentSensor(CURRENT_SENSOR_PIN, CURRENT_ACTIVE_HIGH, CURRENT_DEBOUNCE_MS);
EspNowDriver espNow;
ServerComms server;

bool failureActive = false;
unsigned long failureClearTimer = 0;
unsigned long lastHttpRetry = 0;
bool httpStartPending = false;
bool wifiWasDown = false;

void setup() {
  Serial.begin(115200);
  delay(500);

  PRINT_DEBUG("\n=== MAIN ESP (Current Sensor) Starting ===\n");

  currentSensor.begin();
  PRINT_DEBUG("Current sensor initialized\n");

  uint8_t mac[6];
  WiFi.macAddress(mac);
  PRINT_DEBUG("MAIN ESP MAC: %02X:%02X:%02X:%02X:%02X:%02X\n",
              mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  PRINT_DEBUG("Config format: {0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X}\n",
              mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

  if (!server.connectWiFi(WIFI_SSID, WIFI_PASSWORD)) {
    PRINT_DEBUG("ERROR: WiFi connection failed on first try\n");
    // Give the radio a moment and retry once
    delay(2000);
    if (!server.connectWiFi(WIFI_SSID, WIFI_PASSWORD)) {
      PRINT_DEBUG("ERROR: WiFi still failed. Will keep retrying in loop()\n");
    }
  }
  if (WiFi.status() == WL_CONNECTED) {
    PRINT_DEBUG("WiFi channel: %d  RSSI: %d dBm\n", WiFi.channel(), WiFi.RSSI());
  }

  if (!espNow.begin(SECONDARY_MAC)) {
    PRINT_DEBUG("ERROR: ESP-NOW init failed\n");
  }

  espNow.onRecv([](const char *payload, int len) {
    char buf[128];
    if (len < (int)sizeof(buf)) {
      memcpy(buf, payload, len);
      buf[len] = '\0';
      PRINT_DEBUG("Raw ESP-NOW: %s\n", buf);
    }
  });

  PRINT_DEBUG("=== Setup complete ===\n\n");
}

void loop() {
  static unsigned long loopCount = 0;
  static unsigned long lastStatus = 0;
  static unsigned long lastLoop = 0;
  static unsigned long lastReconnect = 0;
  static unsigned long lastRssiLog = 0;

  loopCount++;

  unsigned long now = millis();
  if (now - lastLoop < 50) return;
  lastLoop = now;

  currentSensor.update();
  bool currentFault = currentSensor.isFault();

  bool irValid = espNow.hasValidData();
  int irState = espNow.getLastIrState();
  int irIdle = irValid ? espNow.getLastIrIdle() : -1;

  bool shouldFail = currentFault || (irValid && irIdle == 1);

  if (now - lastStatus >= 1000) {
    lastStatus = now;
    PRINT_DEBUG("Loop #%lu | Current: %d | IR State: %d | IR Idle: %d | IR Valid: %d | Should Fail: %d | Active: %d | WiFi: %d\n",
                loopCount, currentFault ? 1 : 0, irState, irIdle, irValid ? 1 : 0,
                shouldFail ? 1 : 0, failureActive ? 1 : 0,
                server.isWiFiConnected() ? 1 : 0);
  }

  if (!server.isWiFiConnected()) {
    wifiWasDown = true;
    if (now - lastReconnect >= 30000) {  // 30s cooldown (reconnect itself takes ~10s)
      lastReconnect = now;
      PRINT_DEBUG("[LOOP] WiFi down, reconnecting...\n");
      server.reconnectWiFi(WIFI_SSID, WIFI_PASSWORD);
    }
  } else if (wifiWasDown) {
    wifiWasDown = false;
    PRINT_DEBUG("[LOOP] WiFi reconnected! IP: %s  RSSI: %d dBm\n",
                WiFi.localIP().toString().c_str(), WiFi.RSSI());
    espNow.reAddPeer(SECONDARY_MAC);
    // Re-confirm power save is off after reconnect
    esp_wifi_set_ps(WIFI_PS_NONE);
  }

  // Periodic RSSI logging (every 30s when connected)
  if (server.isWiFiConnected() && now - lastRssiLog >= 30000) {
    lastRssiLog = now;
    int rssi = WiFi.RSSI();
    if (rssi < -80) {
      PRINT_DEBUG("[WiFi] WARNING: Weak signal! RSSI: %d dBm\n", rssi);
    }
  }

  if (shouldFail && !failureActive && !httpStartPending) {
    httpStartPending = true;
    lastHttpRetry = now;

    PRINT_DEBUG("[EVENT] FAILURE START detected\n");

    if (server.sendFailureStart(MACHINE_ID)) {
      failureActive = true;
      failureClearTimer = 0;
      httpStartPending = false;
      PRINT_DEBUG("[LOOP %lu] Failure start sent OK\n", loopCount);
    } else {
      PRINT_DEBUG("[LOOP %lu] Failure start FAILED, will retry\n", loopCount);
    }
  }

  if (httpStartPending && !failureActive) {
    if (now - lastHttpRetry >= HTTP_RETRY_INTERVAL_MS) {
      lastHttpRetry = now;
      PRINT_DEBUG("[RETRY] Retrying failure-start\n");
      if (server.sendFailureStart(MACHINE_ID)) {
        failureActive = true;
        failureClearTimer = 0;
        httpStartPending = false;
        PRINT_DEBUG("[RETRY] Failure start sent OK\n");
      }
    }
  }

  if (!shouldFail && failureActive) {
    if (failureClearTimer == 0) {
      failureClearTimer = now;
    }

    unsigned long elapsed = now - failureClearTimer;
    if (elapsed >= FAILURE_CLEAR_DEBOUNCE_MS) {
      failureActive = false;
      failureClearTimer = 0;

      PRINT_DEBUG("[EVENT] FAILURE STOP detected\n");

      if (server.sendFailureStop(MACHINE_ID)) {
        PRINT_DEBUG("[LOOP %lu] Failure stop sent OK\n", loopCount);
      } else {
        PRINT_DEBUG("[LOOP %lu] Failure stop FAILED, will retry\n", loopCount);
        failureActive = true;
        lastHttpRetry = now;
      }
    }
  } else if (shouldFail && failureActive) {
    failureClearTimer = 0;
  }

  if (failureActive && !shouldFail && failureClearTimer == 0) {
    failureClearTimer = now;
  }

  if (httpStartPending && shouldFail && failureActive) {
    httpStartPending = false;
  }
}
