#pragma once
#include <Arduino.h>
#include <functional>

// Lightweight wrapper around ESP-NOW functionality.
// Provides:
// - initialization helper (WiFi + esp_now)
// - adding a peer (so you can target a single receiver)
// - simple send() API and a send-callback registration
class EspNowDriver {
public:
  using SendCallback = std::function<void(bool ok)>;

  // Construct the driver instance. Call begin() to initialize ESP-NOW.
  EspNowDriver();

  // Initialize ESP-NOW and optionally add the peer MAC to the peer table.
  // Returns true on success.
  bool begin(const uint8_t peerMac[6]);

  // Send raw bytes to peerMac. If peerMac==nullptr the driver will send
  // to broadcast (FF:FF:...)
  bool send(const uint8_t *peerMac, const uint8_t *data, size_t len);

  // Register a callback to be notified when a send completes.
  void onSend(SendCallback cb);

private:
  SendCallback _onSend;
};
