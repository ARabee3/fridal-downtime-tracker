#pragma once
#include <Arduino.h>

// Server communication driver for sending failure events to the Fridal backend.
// Handles WiFi connection and HTTP requests.
class ServerComms {
public:
  ServerComms();

  // Connect to WiFi. Returns true on success.
  bool connectWiFi(const char *ssid, const char *password);

  // Check if WiFi is connected
  bool isWiFiConnected() const;

  // Send a "failure start" event to the server
  // Returns true if the request was successful
  bool sendFailureStart(const char *machineId, const char *timestamp = nullptr);

  // Send a "failure stop" event to the server
  // Returns true if the request was successful
  bool sendFailureStop(const char *machineId);

private:
  bool _sendRequest(const char *endpoint, const char *payload);
};
