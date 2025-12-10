#include <Arduino.h>

extern "C"
{
#include "arduinogotchi_core/bitmaps.h"
#include "arduinogotchi_core/tamalib.h"
#include "arduinogotchi_core/hw.h"
#include "arduinogotchi_core/cpu.h"
#include "arduinogotchi_core/hal.h"
}

#include "VideoService.h"
#include "InputService.h"
#include "AudioService.h"
#include "TamaHost.h"
#include "esp_timer.h"

/**** Tama Setting ****/
#define TAMA_DISPLAY_FRAMERATE 3

/**********************/

// Service vidéo
static VideoService video;

// Service input
static InputService input;

// Service Audio
static AudioService audio;

// Service TamaHost
static TamaHost host(video, input);

// Glue audio utilisée par TamaHost
void espgotchi_hal_set_frequency(u32_t freq)
{
  audio.setFrequency(freq);
}

void espgotchi_hal_play_frequency(bool_t en)
{
  if (en)
    audio.play();
  else
    audio.stop();
}

void setup()
{
  Serial.begin(115200);
  delay(200);

  // Init touch + hw
  input.begin();
  hw_init();

  // Vidéo
  // Brancher InputService dans VideoService pour la barre de boutons
  video.setInputService(&input);
  // Tout ce qui touche l'écran passe par VideoService
  video.initDisplay();
  video.begin();

  // Splash écran de boot
  video.showSplash("Kharn27 EspGotchi");
  delay(2500);
  video.clearScreen();

  // Audio
  audio.begin();

  // petit test son (optionnel)
  audio.setFrequency(880);
  audio.play();
  delay(120);
  audio.stop();
  delay(80);
  audio.setFrequency(1320);
  audio.play();
  delay(120);
  audio.stop();

  // Hôte TamaLIB (HAL, temps virtuel, handler, etc.)
  host.begin(TAMA_DISPLAY_FRAMERATE, 1000000);

  Serial.println("[Espgotchi] Step Refactoring Service started.");
}

void loop()
{
  host.loopOnce();
}
