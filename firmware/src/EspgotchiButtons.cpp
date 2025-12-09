#include "EspgotchiButtons.h"

extern "C"
{
#include "arduinogotchi_core/hw.h"
}

#include "EspgotchiInputC.h"

void espgotchi_buttons_update()
{
  espgotchi_input_update(); // <-- AJOUT IMPORTANT

  uint8_t held = espgotchi_input_peek_held();

  // Par défaut tout relâché
  hw_set_button(BTN_LEFT, BTN_STATE_RELEASED);
  hw_set_button(BTN_MIDDLE, BTN_STATE_RELEASED);
  hw_set_button(BTN_RIGHT, BTN_STATE_RELEASED);

  // Applique l'appui maintenu
  switch (held)
  {
  case 1:
    hw_set_button(BTN_LEFT, BTN_STATE_PRESSED);
    break;
  case 2:
    hw_set_button(BTN_MIDDLE, BTN_STATE_PRESSED);
    break;
  case 3:
    hw_set_button(BTN_RIGHT, BTN_STATE_PRESSED);
    break;
  default:
    break;
  }
}
