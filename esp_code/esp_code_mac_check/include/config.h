#pragma once

// WiFi credentials for the network that can reach the Fridal backend
#define WIFI_SSID "OHS"
#define WIFI_PASSWORD "LOOOL911"

// Backend server settings
#define SERVER_HOST "10.238.6.240"
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
