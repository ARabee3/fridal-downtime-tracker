#include <Arduino.h>
#include "../include/config.h"
#include "CurrentSensor.h"
#include "EspNowDriver.h"
#include "ServerComms.h"

// Define static peers from config
static const uint8_t SECONDARY_MAC[6] = SECONDARY_PEER_MAC;

// Global driver instances
CurrentSensor currentSensor(CURRENT_SENSOR_PIN, CURRENT_ACTIVE_HIGH, CURRENT_DEBOUNCE_MS);
EspNowDriver espNow;
ServerComms server;

// Failure state tracking
bool failureActive = false;
unsigned long failureClearTimer = 0;

void setup() {
  Serial.begin(115200);
  delay(500);

  #if BOARD_DEBUG
  Serial.println("\n\n=== MAIN ESP (Current Sensor) Starting ===");
  #endif

  // Initialize current sensor
  currentSensor.begin();
  #if BOARD_DEBUG
  Serial.println("Current sensor initialized");
  #endif

  // Connect to WiFi
  if (!server.connectWiFi(WIFI_SSID, WIFI_PASSWORD)) {
    #if BOARD_DEBUG
    Serial.println("ERROR: WiFi connection failed");
    #endif
  }

  // Initialize ESP-NOW to receive IR messages from secondary
  if (!espNow.begin(SECONDARY_MAC)) {
    #if BOARD_DEBUG
    Serial.println("ERROR: ESP-NOW init failed");
    #endif
  }

  // Optional: register raw message callback
  espNow.onRecv([](const char *payload, int len) {
    #if BOARD_DEBUG
    char buf[128];
    if (len < sizeof(buf)) {
      memcpy(buf, payload, len);
      buf[len] = '\0';
      Serial.printf("Raw ESP-NOW: %s\n", buf);
    }
    #endif
  });

  #if BOARD_DEBUG
  Serial.println("=== Setup complete ===\n");
  #endif
}

void loop() {
  // Update the current sensor
  currentSensor.update();
  bool currentFault = currentSensor.isFault();

  // Get the latest IR state from ESP-NOW (updated via callback)
  int irState = espNow.getLastIrState();
  int irIdle = espNow.getLastIrIdle();

  // Determine overall failure condition:
  // Failure occurs if:
  //   - Current sensor detects a fault, OR
  //   - IR sensor has been idle (no bottle detection for 30+ seconds)
  bool shouldFail = currentFault || (irIdle == 1);

  #if BOARD_DEBUG
  static unsigned long lastDebug = 0;
  if (millis() - lastDebug > 5000) {
    lastDebug = millis();
    Serial.printf("Current: %d | IR State: %d | IR Idle: %d | Should Fail: %d | Active: %d\n",
                  currentFault ? 1 : 0, irState, irIdle, shouldFail ? 1 : 0, failureActive ? 1 : 0);
  }
  #endif

  // State transition logic
  if (shouldFail && !failureActive) {
    // Transition: no failure -> failure detected
    failureActive = true;
    failureClearTimer = 0;

    #if BOARD_DEBUG
    Serial.println("[EVENT] FAILURE START detected");
    #endif

    // Send failure-start to server
    server.sendFailureStart(MACHINE_ID);

  } else if (!shouldFail && failureActive) {
    // Transition: failure -> potential clear
    // Use debounce timer to avoid rapid on/off chatter
    if (failureClearTimer == 0) {
      failureClearTimer = millis();
    }

    unsigned long elapsed = millis() - failureClearTimer;
    if (elapsed >= FAILURE_CLEAR_DEBOUNCE_MS) {
      // Debounce window expired; failure is truly cleared
      failureActive = false;
      failureClearTimer = 0;

      #if BOARD_DEBUG
      Serial.println("[EVENT] FAILURE STOP detected");
      #endif

      // Send failure-stop to server
      server.sendFailureStop(MACHINE_ID);
    }
  } else {
    // No state change; reset debounce timer
    failureClearTimer = 0;
  }

  delay(50);
}
