#include "EspgotchiInput.h"
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>
#include <SPI.h>

// On réutilise tes defines et ton mapping déjà validés
#ifndef TOUCH_X_MIN
#define TOUCH_X_MIN  200
#endif
#ifndef TOUCH_X_MAX
#define TOUCH_X_MAX  3800
#endif
#ifndef TOUCH_Y_MIN
#define TOUCH_Y_MIN  200
#endif
#ifndef TOUCH_Y_MAX
#define TOUCH_Y_MAX  3800
#endif

static const int SCREEN_W = 320;
static const int SCREEN_H = 240;

static const int BAR_H = 50;
static const int BAR_Y = SCREEN_H - BAR_H;
static const int BTN_W = SCREEN_W / 4;

// Touch global minimal (pas d’UI ici)
static SPIClass touchSPI(VSPI);
static XPT2046_Touchscreen ts(TOUCH_CS, TOUCH_IRQ);

static bool mapTouchToScreen(const TS_Point &p, int &sx, int &sy) {
  if (p.z < 50) return false;

  sx = map(p.x, TOUCH_X_MIN, TOUCH_X_MAX, 0, SCREEN_W - 1);
  sy = map(p.y, TOUCH_Y_MIN, TOUCH_Y_MAX, 0, SCREEN_H - 1);

  if (sx < 0) sx = 0;
  if (sx >= SCREEN_W) sx = SCREEN_W - 1;
  if (sy < 0) sy = 0;
  if (sy >= SCREEN_H) sy = SCREEN_H - 1;

  return true;
}

VButton EspgotchiInput::consumeAny() {
  VButton pressed;
  if (readStablePress(pressed)) {
    return pressed;
  }
  return VButton::NONE;
}

static VButton hitTestButton(int x, int y) {
  if (y < BAR_Y) return VButton::NONE;
  if (x < BTN_W)         return VButton::LEFT;
  if (x < 2 * BTN_W)     return VButton::OK;
  if (x < 3 * BTN_W)     return VButton::RIGHT;
  return VButton::LR;  // dernier quart de l’écran
}

void EspgotchiInput::begin() {
  touchSPI.begin(TOUCH_SCK, TOUCH_MISO, TOUCH_MOSI, TOUCH_CS);
  ts.begin(touchSPI);
  ts.setRotation(1);
}

bool EspgotchiInput::readStablePress(VButton &outPressed) {
  outPressed = VButton::NONE;
  VButton now = VButton::NONE;

  if (ts.touched()) {
    TS_Point p = ts.getPoint();
    int sx, sy;
    if (mapTouchToScreen(p, sx, sy)) {
      // on mémorise la position écran pour l'UI
      lastX = (uint16_t)sx;
      lastY = (uint16_t)sy;
      lastHasXY = true;
      lastDown = true;

      now = hitTestButton(sx, sy);
    } else {
      lastDown = true;
    }
  } else {
    lastDown = false;
  }

  uint32_t ms = millis();

  if (now != db.lastRead) {
    db.lastRead = now;
    db.lastChangeMs = ms;
    db.fired = false;
  }

  if ((ms - db.lastChangeMs) > 60) {
    db.stable = db.lastRead;
  }

  // Held state
  held = db.lastRead;

  if (db.stable != VButton::NONE && !db.fired) {
    db.fired = true;
    outPressed = db.stable;
    return true;
  }

  if (db.lastRead == VButton::NONE) {
    db.fired = false;
    db.stable = VButton::NONE;
  }

  return false;
}

void EspgotchiInput::update() {
  VButton pressed;
  readStablePress(pressed);
  // on ne fait rien ici, on consomme via consumeX()
}

bool EspgotchiInput::consumeLeft() {
  VButton pressed;
  if (readStablePress(pressed) && pressed == VButton::LEFT) return true;
  return false;
}
bool EspgotchiInput::consumeOk() {
  VButton pressed;
  if (readStablePress(pressed) && pressed == VButton::OK) return true;
  return false;
}
bool EspgotchiInput::consumeRight() {
  VButton pressed;
  if (readStablePress(pressed) && pressed == VButton::RIGHT) return true;
  return false;
}
