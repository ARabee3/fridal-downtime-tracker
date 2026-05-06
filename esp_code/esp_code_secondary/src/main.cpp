#include <Arduino.h>
#include <WiFi.h>
#include "../include/config.h"
#include "IRSensor.h"
#include "EspNowDriver.h"

// MAIN_MAC: the 6-byte MAC of the main ESP receiver. This comes from config.h
static const uint8_t MAIN_MAC[6] = MAIN_PEER_MAC;

// Instantiate IR sensor driver and ESPNOW driver
IRSensor ir(IR_PIN, IR_ACTIVE_HIGH, IR_DEBOUNCE_MS);
EspNowDriver comms;

// Track whether we've already reported an "idle" failure so we don't spam
bool idleReported = false;

void setup() {

#if BOARD_DEBUG
  Serial.begin(115200);
  delay(500);
#endif

PRINT_DEBUG("Secondary ESP (IR) starting...\n");

  // Initialize the IR sensor driver (configures pin, samples initial state)
  ir.begin();

  uint8_t mac[6];
  WiFi.macAddress(mac);
  PRINT_DEBUG("SECONDARY ESP MAC: %02X:%02X:%02X:%02X:%02X:%02X\n", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  PRINT_DEBUG("Config format: {0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X}\n", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

  // Connect to WiFi to sync on the same channel as main ESP
  PRINT_DEBUG("Connecting to WiFi: %s\n", WIFI_SSID);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  
  int attempts = 0;
  const int maxAttempts = 20;
  while (WiFi.status() != WL_CONNECTED && attempts < maxAttempts) {
    delay(500);
    PRINT_DEBUG(".");
    attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    PRINT_DEBUG("\nWiFi connected! Channel: %d\n", WiFi.channel());
  } else {
    PRINT_DEBUG("\nWiFi connection failed (ESP-NOW may not work)\n");
  }

  // Initialize ESP-NOW and try to add the main peer (for direct sends)
  if (!comms.begin(MAIN_MAC)) {
    PRINT_DEBUG("ESP-NOW init failed\n");
	}

	// Optional: print send result for debugging
	comms.onSend([](bool ok) {
		PRINT_DEBUG("ESP-NOW send %s\n", ok ? "OK" : "FAIL");
	});
}

unsigned long lastHeartbeat = 0;

// sendIrState(): build a tiny JSON-like string and send to the main ESP.
// Payload fields:
// - type: "ir" (so receiver can distinguish message types)
// - state: 1 when object present, 0 otherwise
// - idle: 1 when idle/failure detected, 0 otherwise
void sendIrState(bool state, bool idle) {
	char buf[64];
	snprintf(buf, sizeof(buf), "{\"type\":\"ir\",\"state\":%d,\"idle\":%d}", state ? 1 : 0, idle ? 1 : 0);
	comms.send(MAIN_MAC, (const uint8_t *)buf, strlen(buf) + 1);
	PRINT_DEBUG("Sent: %s\n", buf);
}

void loop() {
	// Update the IR driver; 'changed' is true when debounced logical state toggles
	bool changed = ir.update();
	bool state = ir.getState();

	// Check whether the IR has been "idle" (no state changes) for configured timeout
	bool idle = ir.isIdleTooLong(IR_IDLE_TIMEOUT_SEC);

	if (changed) {
		// When the sensor toggles (bottle passing), send the new state immediately.
		if (idleReported && !idle) idleReported = false;
		sendIrState(state, idle);
	}

	// If idle condition appears for the first time, report start of failure
	if (idle && !idleReported) {
		idleReported = true;
		sendIrState(state, true);
	}

	// If idle was previously reported but now cleared, report stop of failure
	if (!idle && idleReported) {
		idleReported = false;
		sendIrState(state, false);
	}

	// Send IR status every 1 second to keep main ESP updated
	if (millis() - lastHeartbeat > 1000UL) {
		lastHeartbeat = millis();
		sendIrState(state, idle);
	}

	// Small delay to avoid busy-looping; the IR driver handles timing internally
	delay(10);
}