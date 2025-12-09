#include "EspgotchiInputC.h"
#include "EspgotchiInput.h"

static EspgotchiInput g_input;

void espgotchi_input_begin() {
  g_input.begin();
}

void espgotchi_input_update() {
  g_input.update();
}


// 0 = NONE, 1 = LEFT, 2 = OK, 3 = RIGHT
uint8_t espgotchi_input_consume_any() {
  VButton b = g_input.consumeAny();
  return static_cast<uint8_t>(b);
}

uint8_t espgotchi_input_peek_held() {
  VButton b = g_input.peekHeld();
  return static_cast<uint8_t>(b);
}

uint8_t espgotchi_input_get_last_touch(uint16_t* x, uint16_t* y, uint8_t* down) {
  if (!x || !y || !down) return 0;

  uint16_t lx, ly;
  bool ldown;
  bool ok = g_input.getLastTouch(lx, ly, ldown);
  *x = lx;
  *y = ly;
  *down = ldown ? 1 : 0;
  return ok ? 1 : 0;
}


