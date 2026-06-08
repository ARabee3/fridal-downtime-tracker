#include "EspNowDriver.h"
#include <WiFi.h>
#include <esp_now.h>
#include "../include/config.h"

static EspNowDriver *s_instance = nullptr;

EspNowDriver::EspNowDriver() { s_instance = this; }

bool EspNowDriver::begin(const uint8_t peerMac[6]) {
  if (esp_now_init() != ESP_OK) {
    PRINT_DEBUG("[ESP-NOW] Init failed\n");
    return false;
  }

  esp_now_register_send_cb([](const uint8_t *mac_addr, esp_now_send_status_t status) {
    if (!s_instance) return;
    if (s_instance->_onSend) s_instance->_onSend(status == ESP_NOW_SEND_SUCCESS);
  });

  esp_now_peer_info_t peer = {};
  memcpy(peer.peer_addr, peerMac, 6);
  peer.channel = 0;
  peer.encrypt = false;
  if (esp_now_add_peer(&peer) != ESP_OK) {
    PRINT_DEBUG("[ESP-NOW] Warning: failed to add peer\n");
  } else {
    PRINT_DEBUG("[ESP-NOW] Peer added successfully\n");
  }
  return true;
}

bool EspNowDriver::send(const uint8_t *peerMac, const uint8_t *data, size_t len) {
  const uint8_t broadcast[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
  const uint8_t *target = peerMac ? peerMac : broadcast;
  esp_err_t err = esp_now_send(target, data, len);
  return (err == ESP_OK);
}

void EspNowDriver::onSend(SendCallback cb) { _onSend = cb; }
