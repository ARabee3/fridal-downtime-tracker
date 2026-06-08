#pragma once
#include <Arduino.h>

// Simple IR sensor driver encapsulated as a class.
// Responsibilities:
// - Read a digital input pin connected to the IR sensor
// - Debounce rapid fluctuations
// - Keep track of the last time the logical state changed
// - Provide a helper to detect "idle" periods (no state changes)
class IRSensor {
public:
  // pin: GPIO pin number
  // activeHigh: whether HIGH means object present
  // debounceMs: debounce window in milliseconds
  IRSensor(uint8_t pin, bool activeHigh = true, unsigned long debounceMs = 50);

  // Call once from setup() to configure the input and initialize state
  void begin();

  // Call frequently from loop(); returns true if the logical state changed
  bool update();

  // Returns the current logical state: true = object detected (per polarity)
  bool getState() const;

  // Millis timestamp of the last time the logical state changed
  unsigned long lastChangeMillis() const;

  // Returns true if no state changes occurred for 'timeoutSec' seconds
  bool isIdleTooLong(unsigned long timeoutSec) const;

private:
  uint8_t _pin;
  bool _activeHigh;
  unsigned long _debounceMs;
  bool _state;                     // debounced logical state
  unsigned long _lastDebounceMillis; // last time a raw change was observed
  unsigned long _lastChangeMillis;  // last time the debounced state actually changed
  int _lastRaw;                     // last raw digitalRead value
};
