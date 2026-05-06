#pragma once
#include <Arduino.h>
#include <functional>

// Lightweight wrapper around ESP-NOW for the main ESP (receiver).
// Provides:
// - initialization and peer management
// - message reception with callback
// - storage of the most recent IR sensor state from the secondary
class EspNowDriver {
public:
  using RecvCallback = std::function<void(const char *payload, int len)>;
  
  // Construct the driver instance
  EspNowDriver();

  // Initialize ESP-NOW and add the secondary peer
  bool begin(const uint8_t secondaryMac[6]);

  // Register a callback to receive messages
  void onRecv(RecvCallback cb);

  // Get the last received IR state (0/1) from secondary
  // Returns -1 if no message received yet
  int getLastIrState() const { return _lastIrState; }

  // Get the last received idle flag (0/1) from secondary
  // Returns -1 if no message received yet
  int getLastIrIdle() const { return _lastIrIdle; }

private:
  RecvCallback _onRecv;
  int _lastIrState;
  int _lastIrIdle;
};
