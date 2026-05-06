#include "EspNowDriver.h"
#include <WiFi.h>
#include <esp_now.h>
#include "../include/config.h"

// Single static pointer used by the C-style esp_now callback to reach
// the C++ instance method.
static EspNowDriver *s_instance = nullptr;

EspNowDriver::EspNowDriver() { s_instance = this; }

// begin(): set WiFi mode to station and initialize ESP-NOW.
// Registers a send-complete callback and attempts to add the provided peer
// to the internal peer table. Adding a peer is optional; you can still send
// to broadcast addresses if adding fails.
bool EspNowDriver::begin(const uint8_t peerMac[6]) {
  WiFi.mode(WIFI_STA);
  if (esp_now_init() != ESP_OK) {
    return false;
  }

  // Register a thin C callback which forwards the status to the C++ instance
  esp_now_register_send_cb([](const uint8_t *mac_addr, esp_now_send_status_t status) {
    if (!s_instance) return;
    if (s_instance->_onSend) s_instance->_onSend(status == ESP_NOW_SEND_SUCCESS);
  });

  // Try to add the peer to the peer list so broadcasts are not required.
  esp_now_peer_info_t peer = {};
  memcpy(peer.peer_addr, peerMac, 6);
  peer.channel = 0;
  peer.encrypt = false;
  if (esp_now_add_peer(&peer) != ESP_OK) {
    // Warning: failure to add a peer is non-fatal; we can still send to broadcast
    PRINT_DEBUG("Warning: failed to add ESPNOW peer\n");
  }
  return true;
}

// send(): convenience wrapper around esp_now_send. If peerMac is null,
// sends to broadcast address (FF:FF:...)
bool EspNowDriver::send(const uint8_t *peerMac, const uint8_t *data, size_t len) {
  const uint8_t broadcast[6] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
  const uint8_t *target = peerMac ? peerMac : broadcast;
  esp_err_t err = esp_now_send(target, data, len);
  return (err == ESP_OK);
}

// onSend(): register a C++ callback to receive send-completion notifications
void EspNowDriver::onSend(SendCallback cb) { _onSend = cb; }
