#include "EspNowDriver.h"
#include <WiFi.h>
#include <esp_now.h>
#include "../include/config.h"

static EspNowDriver *s_instance = nullptr;

EspNowDriver::EspNowDriver() {
  s_instance = this;
  _lastIrState = -1;
  _lastIrIdle = -1;
  _lastRecvMillis = 0;
}

bool EspNowDriver::begin(const uint8_t secondaryMac[6]) {
  if (esp_now_init() != ESP_OK) {
    PRINT_DEBUG("[ESP-NOW] Init failed\n");
    return false;
  }

  esp_now_register_recv_cb([](const uint8_t *mac, const uint8_t *data, int len) {
    if (!s_instance) return;

    if (s_instance->_onRecv) {
      s_instance->_onRecv((const char *)data, len);
    }

    char buf[128];
    if (len >= (int)sizeof(buf) - 1) return;
    memcpy(buf, data, len);
    buf[len] = '\0';

    int state = -1, idle = -1;

    const char *statePtr = strstr(buf, "\"state\":");
    if (statePtr) {
      statePtr += 8;
      while (*statePtr == ' ' || *statePtr == '\t') statePtr++;
      state = atoi(statePtr);
    }

    const char *idlePtr = strstr(buf, "\"idle\":");
    if (idlePtr) {
      idlePtr += 7;
      while (*idlePtr == ' ' || *idlePtr == '\t') idlePtr++;
      idle = atoi(idlePtr);
    }

    if (state >= 0 && idle >= 0) {
      s_instance->_lastIrState = state;
      s_instance->_lastIrIdle = idle;
      s_instance->_lastRecvMillis = millis();
      PRINT_DEBUG("[ESP-NOW] IR: state=%d idle=%d\n", state, idle);
    }
  });

  esp_now_peer_info_t peer = {};
  memcpy(peer.peer_addr, secondaryMac, 6);
  peer.channel = 0;
  peer.encrypt = false;
  if (esp_now_add_peer(&peer) != ESP_OK) {
    PRINT_DEBUG("[ESP-NOW] Warning: failed to add secondary peer\n");
  } else {
    PRINT_DEBUG("[ESP-NOW] Secondary peer added\n");
  }

  return true;
}

bool EspNowDriver::reAddPeer(const uint8_t secondaryMac[6]) {
  if (esp_now_is_peer_exist(secondaryMac)) return true;

  esp_now_peer_info_t peer = {};
  memcpy(peer.peer_addr, secondaryMac, 6);
  peer.channel = 0;
  peer.encrypt = false;
  if (esp_now_add_peer(&peer) != ESP_OK) {
    PRINT_DEBUG("[ESP-NOW] Failed to re-add peer\n");
    return false;
  }
  PRINT_DEBUG("[ESP-NOW] Peer re-added after WiFi reconnect\n");
  return true;
}

void EspNowDriver::onRecv(RecvCallback cb) { _onRecv = cb; }

unsigned long EspNowDriver::getLastRecvAge() const {
  if (_lastRecvMillis == 0) return 0xFFFFFFFF;
  unsigned long now = millis();
  return (now >= _lastRecvMillis) ? (now - _lastRecvMillis) : 0;
}

bool EspNowDriver::hasValidData() const {
  unsigned long age = getLastRecvAge();
  return age < ESPNOW_STALE_MS;
}
