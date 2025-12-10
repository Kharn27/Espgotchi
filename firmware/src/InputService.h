#pragma once

#include <Arduino.h>
#include "EspgotchiInput.h"

extern "C" {
#include "arduinogotchi_core/hw.h"
}

// Service d'entrée haut niveau pour EspGotchi
// - encapsule EspgotchiInput (touch + mapping L/OK/R)
// - met à jour les boutons Tama (hw_set_button)
// - expose held + dernier touch pour l'app
class InputService {
public:
  void begin();
  void update();

  // 0 = NONE, 1 = LEFT, 2 = OK, 3 = RIGHT (comme espgotchi_input_xxx)
  uint8_t getHeld() const;

  // Renvoie 1 si on a une position connue, 0 sinon
  // down = 1 si l'écran est pressé, 0 sinon
  uint8_t getLastTouch(uint16_t &x, uint16_t &y, uint8_t &down) const;

private:
  EspgotchiInput input;
};
