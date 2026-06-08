#include <Arduino.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include "../include/config.h"
#include "IRSensor.h"
#include "EspNowDriver.h"

static const uint8_t MAIN_MAC[6] = MAIN_PEER_MAC;

IRSensor ir(IR_PIN, IR_ACTIVE_HIGH, IR_DEBOUNCE_MS);
EspNowDriver comms;

bool idleReported = false;

void setup() {
#if BOARD_DEBUG
  Serial.begin(115200);
  delay(500);
#endif

  PRINT_DEBUG("Secondary ESP (IR) starting...\n");

  ir.begin();

  PRINT_DEBUG("Connecting to WiFi: %s\n", WIFI_SSID);
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);

  // Disable LR mode — use standard 802.11 b/g/n
  esp_wifi_set_protocol(WIFI_IF_STA, WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G | WIFI_PROTOCOL_11N);

  // Disable power save — prevents beacon miss / association expire
  esp_wifi_set_ps(WIFI_PS_NONE);

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  int attempts = 0;
  const int maxAttempts = 40;
  while (WiFi.status() != WL_CONNECTED && attempts < maxAttempts) {
    delay(500);
    PRINT_DEBUG(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    // Re-confirm power save off after connection
    esp_wifi_set_ps(WIFI_PS_NONE);
    PRINT_DEBUG("\nWiFi connected! Channel: %d  RSSI: %d dBm\n", WiFi.channel(), WiFi.RSSI());
  } else {
    PRINT_DEBUG("\nWiFi connection failed (ESP-NOW may not work on wrong channel)\n");
  }

  uint8_t mac[6];
  WiFi.macAddress(mac);
  PRINT_DEBUG("SECONDARY ESP MAC: %02X:%02X:%02X:%02X:%02X:%02X\n",
              mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

  if (!comms.begin(MAIN_MAC)) {
    PRINT_DEBUG("ESP-NOW init failed\n");
  }

  comms.onSend([](bool ok) {
    PRINT_DEBUG("ESP-NOW send %s\n", ok ? "OK" : "FAIL");
  });
}

unsigned long lastHeartbeat = 0;

void sendIrState(bool state, bool idle) {
  char buf[64];
  snprintf(buf, sizeof(buf), "{\"type\":\"ir\",\"state\":%d,\"idle\":%d}",
           state ? 1 : 0, idle ? 1 : 0);
  comms.send(MAIN_MAC, (const uint8_t *)buf, strlen(buf) + 1);
  PRINT_DEBUG("Sent: %s\n", buf);
}

void loop() {
  bool changed = ir.update();
  bool state = ir.getState();
  bool idle = ir.isIdleTooLong(IR_IDLE_TIMEOUT_SEC);

  if (changed) {
    if (idleReported && !idle) idleReported = false;
    sendIrState(state, idle);
  }

  if (idle && !idleReported) {
    idleReported = true;
    sendIrState(state, true);
  }

  if (!idle && idleReported) {
    idleReported = false;
    sendIrState(state, false);
  }

  if (millis() - lastHeartbeat > 1000UL) {
    lastHeartbeat = millis();
    sendIrState(state, idle);
  }

  delay(10);
}
