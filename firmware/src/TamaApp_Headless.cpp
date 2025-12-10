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
#include "TamaHost.h"
#include "esp_timer.h"

/**** Tama Setting ****/
#define TAMA_DISPLAY_FRAMERATE 3
#define BUZZER_PIN 26
#define BUZZER_CH 0

/**********************/

// Service vidéo
static VideoService video;

// Service input
static InputService input;

// Service TamaHost
static TamaHost host(video, input);

// Audio
static uint16_t current_freq = 0;

static void buzzer_init()
{
  pinMode(BUZZER_PIN, OUTPUT);
  ledcSetup(BUZZER_CH, 2000, 10);
  ledcAttachPin(BUZZER_PIN, BUZZER_CH);
  ledcWrite(BUZZER_CH, 0);
}

static void buzzer_play(uint32_t freq)
{
  if (freq == 0)
  {
    ledcWrite(BUZZER_CH, 0);
    return;
  }
  ledcWriteTone(BUZZER_CH, freq);
  ledcWrite(BUZZER_CH, 200);
}

static void buzzer_stop()
{
  ledcWrite(BUZZER_CH, 0);
}

// Glue audio utilisée par TamaHost
void espgotchi_hal_set_frequency(u32_t freq) {
  current_freq = (uint16_t)freq;
}

void espgotchi_hal_play_frequency(bool_t en) {
  if (en) buzzer_play(current_freq);
  else    buzzer_stop();
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

  buzzer_init();

  // test rapide
  buzzer_play(880);
  delay(120);
  buzzer_stop();
  delay(80);
  buzzer_play(1320);
  delay(120);
  buzzer_stop();

  // Hôte TamaLIB (HAL, temps virtuel, handler, etc.)
  host.begin(TAMA_DISPLAY_FRAMERATE, 1000000);

  Serial.println("[Espgotchi] Step Refactoring Service started.");
}

void loop()
{
  host.loopOnce();
}
