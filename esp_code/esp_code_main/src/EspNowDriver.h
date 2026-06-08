#pragma once
#include <Arduino.h>
#include <functional>

class EspNowDriver {
public:
  using RecvCallback = std::function<void(const char *payload, int len)>;

  EspNowDriver();

  bool begin(const uint8_t secondaryMac[6]);

  void onRecv(RecvCallback cb);

  int getLastIrState() const { return _lastIrState; }
  int getLastIrIdle() const { return _lastIrIdle; }

  unsigned long getLastRecvAge() const {
    if (_lastRecvMillis == 0) return 0xFFFFFFFF;
    unsigned long now = millis();
    return (now >= _lastRecvMillis) ? (now - _lastRecvMillis) : 0;
  }

  bool hasValidData() const {
    unsigned long age = getLastRecvAge();
    return age < ESPNOW_STALE_MS;
  }

private:
  RecvCallback _onRecv;
  int _lastIrState;
  int _lastIrIdle;
  unsigned long _lastRecvMillis;
};
