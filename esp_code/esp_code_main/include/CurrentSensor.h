#pragma once
#include <Arduino.h>

// Simple current sensor driver encapsulated as a class.
// Responsibilities:
// - Read a digital input pin connected to the current sensor
// - Debounce rapid fluctuations
// - Provide fault detection based on configured polarity
class CurrentSensor {
public:
  // pin: GPIO pin number
  // activeHigh: whether HIGH means a fault is detected
  // debounceMs: debounce window in milliseconds
  CurrentSensor(uint8_t pin, bool activeHigh = true, unsigned long debounceMs = 50);

  // Call once from setup() to configure the input
  void begin();

  // Call frequently from loop(); returns true if the logical state changed
  bool update();

  // Returns the current fault state: true = fault detected (per polarity)
  bool isFault() const;

  // Returns how many consecutive times the fault has been stable (debounced)
  // Useful for understanding state stability
  unsigned long stableCount() const { return _stableCount; }

private:
  uint8_t _pin;
  bool _activeHigh;
  unsigned long _debounceMs;
  bool _fault;                    // debounced logical fault state
  unsigned long _lastDebounceMillis; // last time a raw change was observed
  int _lastRaw;                   // last raw digitalRead value
  unsigned long _stableCount;     // counter for debugging
};
