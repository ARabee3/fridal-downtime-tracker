#include "IRSensor.h"
#include "../include/config.h"

// Constructor: store configuration, leave runtime variables initialized
IRSensor::IRSensor(uint8_t pin, bool activeHigh, unsigned long debounceMs)
    : _pin(pin), _activeHigh(activeHigh), _debounceMs(debounceMs) {
  _state = false;
  _lastDebounceMillis = 0;
  _lastChangeMillis = 0;
  _lastRaw = -1;
}

// begin(): configure the pin and sample the initial state so callers
// have a meaningful `lastChangeMillis()` value right away.
void IRSensor::begin() {
  pinMode(_pin, INPUT);
  int raw = digitalRead(_pin);
  _lastRaw = raw;

  // Convert raw reading to logical (polarity-aware) state
  _state = (_activeHigh ? (raw == HIGH) : (raw == LOW));

  // Initialize timestamps
  _lastChangeMillis = millis();
  _lastDebounceMillis = millis();
}

// update(): perform debouncing and detect logical state changes.
// Returns true when the debounced logical state actually changes.
bool IRSensor::update() {
  int raw = digitalRead(_pin);
  unsigned long now = millis();

  // If the raw reading changed from last time, reset the debounce timer
  if (raw != _lastRaw) {
    _lastDebounceMillis = now;
    _lastRaw = raw;
  }

  // Only accept the new raw reading as a stable change once the debounce
  // window has elapsed without further raw changes.
  if ((now - _lastDebounceMillis) >= _debounceMs) {
    bool sensed = (_activeHigh ? (raw == HIGH) : (raw == LOW));
    if (sensed != _state) {
      // Logical state changed after debouncing: record timestamp and notify
      _state = sensed;
      _lastChangeMillis = now;
      return true;
    }
  }
  return false;
}

// Current logical (debounced + polarity-corrected) state
bool IRSensor::getState() const { return _state; }

unsigned long IRSensor::lastChangeMillis() const { return _lastChangeMillis; }

// Helper used by higher-level logic: returns true when the sensor has not
// seen any debounced logical state change for >= timeoutSec seconds.
bool IRSensor::isIdleTooLong(unsigned long timeoutSec) const {
  unsigned long now = millis();
  unsigned long diff = (now >= _lastChangeMillis) ? (now - _lastChangeMillis) : 0;
  return (diff >= (timeoutSec * 1000UL));
}
