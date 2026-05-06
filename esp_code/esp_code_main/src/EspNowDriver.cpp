#include "EspNowDriver.h"
#include <WiFi.h>
#include <esp_now.h>
#include "../include/config.h"

static EspNowDriver *s_instance = nullptr;

EspNowDriver::EspNowDriver() {
  s_instance = this;
  _lastIrState = -1;
  _lastIrIdle = -1;
}

// begin(): set WiFi mode to station and initialize ESP-NOW.
// Registers a receive callback and attempts to add the secondary peer.
bool EspNowDriver::begin(const uint8_t secondaryMac[6]) {
  WiFi.mode(WIFI_STA);
  if (esp_now_init() != ESP_OK) {
    return false;
  }

  // Register a C callback which forwards to the C++ instance
  esp_now_register_recv_cb([](const uint8_t *mac, const uint8_t *data, int len) {
    if (!s_instance) return;
    
    // Store the raw message and call user callback
    if (s_instance->_onRecv) {
      s_instance->_onRecv((const char *)data, len);
    }

    // Simple JSON parsing: look for "state" and "idle" fields
    // Payload format: {"type":"ir","state":0/1,"idle":0/1}
    char buf[128];
    if (len < sizeof(buf) - 1) {
      memcpy(buf, data, len);
      buf[len] = '\0';
      
      // Very basic JSON parsing (no dependency on ArduinoJson)
      const char *statePtr = strstr(buf, "\"state\":");
      const char *idlePtr = strstr(buf, "\"idle\":");
      
      if (statePtr && idlePtr) {
        int state = statePtr[8] - '0'; // extract digit after "state":
        int idle = idlePtr[7] - '0';   // extract digit after "idle":
        
        s_instance->_lastIrState = state;
        s_instance->_lastIrIdle = idle;
        
        #if BOARD_DEBUG
        Serial.printf("[ESP-NOW] Received IR: state=%d, idle=%d\n", state, idle);
        #endif
      }
    }
  });

  // Try to add the secondary peer to the peer list
  esp_now_peer_info_t peer = {};
  memcpy(peer.peer_addr, secondaryMac, 6);
  peer.channel = 0;  // 0 = flexible channel
  peer.encrypt = false;
  if (esp_now_add_peer(&peer) != ESP_OK) {
    #if BOARD_DEBUG
    Serial.println("Warning: failed to add secondary peer");
    #endif
  } else {
    #if BOARD_DEBUG
    Serial.println("Secondary peer added successfully");
    #endif
  }
  
  return true;
}

// onRecv(): register a C++ callback to receive raw ESP-NOW messages
void EspNowDriver::onRecv(RecvCallback cb) { _onRecv = cb; }
