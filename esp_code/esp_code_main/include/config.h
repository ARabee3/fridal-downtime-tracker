#pragma once

// ========== PIN and HARDWARE CONFIG ==========
// GPIO pin connected to the current sensor output
#define CURRENT_SENSOR_PIN 18

// Polarity of the current sensor:
// set to 1 (true) if sensor reads HIGH when fault detected
// set to 0 (false) if sensor reads LOW when fault detected
#define CURRENT_ACTIVE_HIGH 0

// Debounce time for the current sensor (ms)
#define CURRENT_DEBOUNCE_MS 100

// ========== WiFi CONFIG ==========
// Replace with your WiFi SSID and password
#define WIFI_SSID "Redmi Note 8 Pro2"
#define WIFI_PASSWORD "@12345678@"

// ========== SERVER CONFIG ==========
// IP or hostname of the Fridal backend server
#define SERVER_HOST "192.168.50.249"
#define SERVER_PORT 3000
#define SERVER_API_PATH "/api/sensor"

// Machine identifier sent with failure events
#define MACHINE_ID "M1"

// ========== FAILURE LOGIC ==========
// If a failure is already active, how long to wait before sending stop (ms)
// Set to 1000 (1 second) for quick response, or higher if you want debouncing
#define FAILURE_CLEAR_DEBOUNCE_MS 2000

// ========== ESP-NOW CONFIG ==========
// The MAC address of the secondary ESP (the IR node).
// Replace with the actual MAC of your secondary board.
//  00:70:07:82:CB:18
#define SECONDARY_PEER_MAC {0x00, 0x70, 0x07, 0x82, 0xCB, 0x18}

// ========== DEBUG ==========
// Set to 1 to enable verbose debug output on Serial
#define BOARD_DEBUG 1

// Debug print macro: use PRINT_DEBUG(fmt, ...) instead of Serial.printf when BOARD_DEBUG is set
#if BOARD_DEBUG
#define PRINT_DEBUG(fmt, ...) Serial.printf(fmt, ##__VA_ARGS__)
#else
#define PRINT_DEBUG(fmt, ...) ((void)0)
#endif
