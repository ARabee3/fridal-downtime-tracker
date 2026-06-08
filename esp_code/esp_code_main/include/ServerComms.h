#pragma once
#include <Arduino.h>

class ServerComms {
public:
  ServerComms();

  bool connectWiFi(const char *ssid, const char *password);
  void reconnectWiFi(const char *ssid, const char *password);
  bool isWiFiConnected() const;

  bool sendFailureStart(const char *machineId, const char *timestamp = nullptr);
  bool sendFailureStop(const char *machineId);

  const char *getLastStopId() const { return _lastStopId; }
  void clearStopId() { _lastStopId[0] = '\0'; }

private:
  bool _sendRequest(const char *endpoint, const char *payload, char *outStopId = nullptr, size_t outLen = 0);
  char _lastStopId[64];
  bool _wifiInitDone = false;
  int _reconnectCount = 0;
};
