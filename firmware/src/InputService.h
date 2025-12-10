#pragma once
#include <Arduino.h>
#include "EspgotchiInputC.h"
#include "EspgotchiButtons.h"

struct TouchState {
  uint16_t x = 0;
  uint16_t y = 0;
  bool down = false;
  bool hasXY = false;
};

class InputService {
public:
  void begin();
  void poll();

  uint8_t held() const { return held_; }
  const TouchState &lastTouch() const { return lastTouch_; }
  bool tapStartedIn(uint16_t x, uint16_t y, uint16_t w, uint16_t h) const;

private:
  uint8_t held_ = 0;
  TouchState lastTouch_{};
  bool tapBegan_ = false;
};
