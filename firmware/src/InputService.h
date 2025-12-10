#pragma once

#include <Arduino.h>
#include "EspgotchiInput.h"

extern "C" {
#include "arduinogotchi_core/hw.h"
}

enum class LogicalButton : uint8_t {
  NONE = 0,

  // Boutons “Tama”
  LEFT,
  OK,
  RIGHT,

  // Boutons propres à Espgotchi
  SPEED,
  DEBUG_CENTER,

  // Ajouter ici plus tard : SETTINGS, MUTE, etc.
  // SETTINGS,
  // MUTE,
};

// Service d'entrée haut niveau pour EspGotchi
// - encapsule EspgotchiInput (touch + mapping L/OK/R)
// - met à jour les boutons Tama (hw_set_button)
// - expose held + dernier touch pour l'app
// - expose aussi des "taps" logiques (SPD, DEBUG, etc.)
class InputService {
public:
  void begin();
  void update();

  // Bouton actuellement "tenu" pour le bas de l’écran (L/OK/R)
  LogicalButton getHeld() const;

  // Renvoie 1 si on a une position connue, 0 sinon
  // down = 1 si l'écran est pressé, 0 sinon
  uint8_t getLastTouch(uint16_t &x, uint16_t &y, uint8_t &down) const;

  // Taps ponctuels (SPD, DEBUG, etc.)
  bool consumeTap(LogicalButton b);

private:
  EspgotchiInput input;

  // Petit état interne pour les taps logiques
  bool _tapPending[8] = {false};  // taille >= nb de LogicalButton
  bool _lastTouchDown = false;
};
