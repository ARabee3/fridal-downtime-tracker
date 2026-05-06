#include "../include/CurrentSensor.h"
#include "../include/config.h"

// Constructor: store configuration, leave runtime variables initialized
CurrentSensor::CurrentSensor(uint8_t pin, bool activeHigh, unsigned long debounceMs)
    : _pin(pin), _activeHigh(activeHigh), _debounceMs(debounceMs) {
  _fault = false;
  _lastDebounceMillis = 0;
  _lastRaw = -1;
  _stableCount = 0;
}

// begin(): configure the pin and sample the initial state
void CurrentSensor::begin() {
  pinMode(_pin, INPUT);
  int raw = digitalRead(_pin);
  _lastRaw = raw;

  // Convert raw reading to logical (polarity-aware) fault state
  _fault = (_activeHigh ? (raw == HIGH) : (raw == LOW));
  _lastDebounceMillis = millis();
  _stableCount = 0;
}

// update(): perform debouncing and detect logical state changes.
// Returns true when the debounced logical fault state actually changes.
bool CurrentSensor::update() {
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
    if (sensed != _fault) {
      // Logical fault state changed after debouncing
      _fault = sensed;
      _stableCount++;
      return true;
    }
  }
  return false;
}

// Current logical (debounced + polarity-corrected) fault state
bool CurrentSensor::isFault() const { return _fault; }
