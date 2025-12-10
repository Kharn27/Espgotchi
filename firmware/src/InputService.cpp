#include "InputService.h"

void InputService::begin() {
  input.begin();

  // Réinitialise l'état interne des taps
  for (uint8_t i = 0; i < 8; ++i) {
    _tapPending[i] = false;
  }
  _lastTouchDown = false;
}

void InputService::update() {
  // 1) Met à jour l'état tactile / boutons virtuels
  input.update();

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
  // enum class VButton : uint8_t { NONE = 0, LEFT, OK, RIGHT };

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
  if (idx >= 8) {
    return false;
  }

  if (_tapPending[idx]) {
    _tapPending[idx] = false;
    return true;
  }

  return false;
}