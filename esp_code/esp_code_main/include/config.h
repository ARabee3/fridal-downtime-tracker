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
#define WIFI_SSID "El-ott"
#define WIFI_PASSWORD "C2@d9d0@yFF#*$"

// ========== SERVER CONFIG ==========
// IP of the laptop/PC running the backend server on the WiFi network.
// Do NOT use 127.0.0.1 — that points to the ESP itself, not your laptop.
// Run `ipconfig` (Windows) or `ip a` (Linux) on the server machine to find it.
#define SERVER_HOST "192.168.0.191"
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
#define SECONDARY_PEER_MAC {0x48, 0xCA, 0x43, 0xAF, 0x8B, 0x78}

// Maximum age (ms) of the last ESP-NOW message before data is considered stale
#define ESPNOW_STALE_MS 5000

// ========== HTTP CONFIG ==========
// Timeout for HTTP requests to the server (ms)
#define HTTP_TIMEOUT_MS 5000

// How often to retry failed HTTP requests in the main loop (ms)
#define HTTP_RETRY_INTERVAL_MS 2000

// ========== DEBUG ==========
// Set to 1 to enable verbose debug output on Serial
#define BOARD_DEBUG 1

// Debug print macro: use PRINT_DEBUG(fmt, ...) instead of Serial.printf when BOARD_DEBUG is set
#if BOARD_DEBUG
#define PRINT_DEBUG(fmt, ...) Serial.printf(fmt, ##__VA_ARGS__)
#else
#define PRINT_DEBUG(fmt, ...) ((void)0)
#endif
