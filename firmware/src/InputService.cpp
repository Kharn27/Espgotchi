#include "InputService.h"

#include "EspgotchiButtons.h"
#include "EspgotchiInputC.h"

void InputService::begin() {
  espgotchi_input_begin();
}

bool InputService::updateButtons() {
  espgotchi_buttons_update();
  uint8_t held = espgotchi_input_peek_held();
  bool changed = held != lastHeld;
  lastHeld = held;
  return changed;
}

uint8_t InputService::peekHeld() const {
  return espgotchi_input_peek_held();
}

bool InputService::getLastTouch(uint16_t &x, uint16_t &y, bool &down) const {
  uint8_t rawDown = 0;
  uint16_t lx = 0;
  uint16_t ly = 0;
  bool ok = espgotchi_input_get_last_touch(&lx, &ly, &rawDown);
  if (ok) {
    cachedX = lx;
    cachedY = ly;
    cachedDown = rawDown != 0;
  }
  x = cachedX;
  y = cachedY;
  down = cachedDown;
  return ok;
}

bool InputService::consumeTouchDown(uint16_t &x, uint16_t &y) {
  bool down = false;
  uint16_t lx = 0;
  uint16_t ly = 0;
  bool hasTouch = getLastTouch(lx, ly, down);

  bool tapped = hasTouch && down && !lastDown;
  lastDown = down;

  if (tapped) {
    x = lx;
    y = ly;
  }
  return tapped;
}
