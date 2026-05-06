#pragma once

// --- Pin and hardware config ---
// The GPIO pin connected to the IR sensor output.
// Change this to match your wiring.
#define IR_PIN 18

// Polarity of the IR sensor output:
// - set to 1 (true) if the sensor reads HIGH when an object is present
// - set to 0 (false) if the sensor reads LOW when an object is present
#define IR_ACTIVE_HIGH 1

// Debounce time in milliseconds for filtering mechanical/electrical bounce
// when the IR sensor changes state.
#define IR_DEBOUNCE_MS 50

// If the IR sensor does not change its logical state for this many seconds,
// the code will consider that the IR beam is "idle" (possible failure).
#define IR_IDLE_TIMEOUT_SEC 30

// --- ESP-NOW / Network ---
// The MAC address of the main ESP (the receiver) that will get messages
// from this secondary IR node. Replace the default all-FF address with
// the actual 6-byte MAC of the main board to target it directly.
// Example: {0x24,0x6F,0x28,0xAA,0xBB,0xCC}
#define MAIN_PEER_MAC {0x00, 0x70, 0x07, 0x82, 0xCA, 0x34}

// Enable verbose debug printing to Serial when set to 1. Useful while testing.
#define BOARD_DEBUG 1

// Debug print macro: use PRINT_DEBUG(fmt, ...) instead of Serial.printf when BOARD_DEBUG is set
#if BOARD_DEBUG
#define PRINT_DEBUG(fmt, ...) Serial.printf(fmt, ##__VA_ARGS__)
#else
#define PRINT_DEBUG(fmt, ...) ((void)0)
#endif
