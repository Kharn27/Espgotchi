#include "InputService.h"

void InputService::begin() {
  input.begin();

  // Réinitialise l'état interne des taps
  for (size_t i = 0; i < sizeof(_tapPending) / sizeof(_tapPending[0]); ++i) {
    _tapPending[i] = false;
  }
  _lastTouchDown = false;
}

void InputService::update() {
  // 1) Met à jour l'état tactile / boutons virtuels
  input.update();

  // 1bis) Détection des taps logiques (SPD + DEBUG_CENTER)
  {
    uint16_t x = 0, y = 0;
    bool bDown = false;

    bool ok = input.getLastTouch(x, y, bDown);
    bool newPress = ok && bDown && !_lastTouchDown;

    if (newPress) {
      // --- Bouton SPD : zone en haut à droite ---
      if (x >= SPEED_BTN_X &&
          x <  SPEED_BTN_X + SPEED_BTN_W &&
          y >= SPEED_BTN_Y &&
          y <  SPEED_BTN_Y + SPEED_BTN_H) {

        _tapPending[static_cast<uint8_t>(LogicalButton::SPEED)] = true;
      }

      // --- Bouton DEBUG "invisible" : zone centrale de l'écran ---
      const uint16_t centerLeft   = SCREEN_W  / 3;
      const uint16_t centerRight  = (SCREEN_W * 2) / 3;
      const uint16_t centerTop    = SCREEN_H  / 3;
      const uint16_t centerBottom = (SCREEN_H * 2) / 3;

      if (x >= centerLeft && x < centerRight &&
          y >= centerTop  && y < centerBottom) {

        _tapPending[static_cast<uint8_t>(LogicalButton::DEBUG_CENTER)] = true;
      }
    }

    _lastTouchDown = bDown;
  }

  // 2) Mappe l'état "held" vers les boutons TamaLib (hw_set_button)
  VButton heldV = input.peekHeld();

  // Par défaut tout relâché
  hw_set_button(BTN_LEFT,   BTN_STATE_RELEASED);
  hw_set_button(BTN_MIDDLE, BTN_STATE_RELEASED);
  hw_set_button(BTN_RIGHT,  BTN_STATE_RELEASED);

  switch (heldV) {
    case VButton::LEFT:
      hw_set_button(BTN_LEFT, BTN_STATE_PRESSED);
      break;
    case VButton::OK:
      hw_set_button(BTN_MIDDLE, BTN_STATE_PRESSED);
      break;
    case VButton::RIGHT:
      hw_set_button(BTN_RIGHT, BTN_STATE_PRESSED);
      break;
    default:
      // NONE -> aucun bouton pressé
      break;
  }
}

LogicalButton InputService::getHeld() const {
  VButton b = input.peekHeld();

  switch (b) {
    case VButton::LEFT:  return LogicalButton::LEFT;
    case VButton::OK:    return LogicalButton::OK;
    case VButton::RIGHT: return LogicalButton::RIGHT;
    default:             return LogicalButton::NONE;
  }
}

uint8_t InputService::getLastTouch(uint16_t &x, uint16_t &y, uint8_t &down) const {
  bool bDown = false;
  bool ok = input.getLastTouch(x, y, bDown);
  down = bDown ? 1 : 0;
  return ok ? 1 : 0;
}

bool InputService::consumeTap(LogicalButton b) {
  uint8_t idx = static_cast<uint8_t>(b);
  const uint8_t kCount = sizeof(_tapPending) / sizeof(_tapPending[0]);
  if (idx >= kCount) {
    return false;
  }

  if (_tapPending[idx]) {
    _tapPending[idx] = false;
    return true;
  }

  return false;
}
