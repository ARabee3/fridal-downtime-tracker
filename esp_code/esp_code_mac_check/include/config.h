#pragma once

// WiFi credentials for the network that can reach the Fridal backend
#define WIFI_SSID "El-ott"
#define WIFI_PASSWORD "12345678"

// Backend server settings — use the laptop's LAN IP, NOT 127.0.0.1
#define SERVER_HOST "192.168.0.191"
#define SERVER_PORT 3000
#define SERVER_API_BASE "/api/sensor"

// Machine id used by the backend
#define MACHINE_ID "M1"

// Debug printing helper
#define BOARD_DEBUG 1
#if BOARD_DEBUG
#define PRINT_DEBUG(fmt, ...) Serial.printf(fmt, ##__VA_ARGS__)
#else
#define PRINT_DEBUG(fmt, ...) ((void)0)
#endif
