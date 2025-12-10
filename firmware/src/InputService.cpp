#include "InputService.h"

void InputService::begin() { espgotchi_input_begin(); }

void InputService::poll() {
  bool previousDown = lastTouch_.down;

  espgotchi_buttons_update();
  held_ = espgotchi_input_peek_held();

  uint16_t x = 0;
  uint16_t y = 0;
  uint8_t down = 0;
  uint8_t hasTouch = espgotchi_input_get_last_touch(&x, &y, &down);

  lastTouch_.x = x;
  lastTouch_.y = y;
  lastTouch_.down = down != 0;
  lastTouch_.hasXY = hasTouch != 0;

  tapBegan_ = (!previousDown && lastTouch_.down && lastTouch_.hasXY);
}

bool InputService::tapStartedIn(uint16_t x, uint16_t y, uint16_t w, uint16_t h) const {
  if (!tapBegan_) return false;
  if (!lastTouch_.hasXY) return false;
  return lastTouch_.x >= x && lastTouch_.x < (x + w) &&
         lastTouch_.y >= y && lastTouch_.y < (y + h);
}
