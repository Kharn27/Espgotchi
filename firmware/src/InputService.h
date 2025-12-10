#pragma once

#include <Arduino.h>

class InputService {
public:
  void begin();
  bool updateButtons();
  uint8_t peekHeld() const;
  bool getLastTouch(uint16_t &x, uint16_t &y, bool &down) const;
  bool consumeTouchDown(uint16_t &x, uint16_t &y);

private:
  bool lastDown = false;
  mutable uint16_t cachedX = 0;
  mutable uint16_t cachedY = 0;
  mutable bool cachedDown = false;
  uint8_t lastHeld = 255;
};
