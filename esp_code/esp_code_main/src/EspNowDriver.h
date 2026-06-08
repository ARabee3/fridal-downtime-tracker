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

  unsigned long getLastRecvAge() const;
  bool hasValidData() const;

private:
  RecvCallback _onRecv;
  int _lastIrState;
  int _lastIrIdle;
  unsigned long _lastRecvMillis;
};
