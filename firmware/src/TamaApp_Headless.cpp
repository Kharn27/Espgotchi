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

// time scaling (timeMult doit être visible par VideoService.cpp)
uint8_t timeMult = 1; // 1,2,4,etc...
static uint64_t baseRealUs = 0;
static uint64_t baseVirtualUs = 0;

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

// ---- HAL impl minimale ----
static void hal_halt(void) {}

static void hal_log(log_level_t level, char *buff, ...)
{
  // Simple log brut
  Serial.println(buff);
}

static void set_time_mult(uint8_t newMult)
{
  uint64_t nowReal = (uint64_t)esp_timer_get_time();

  // On "fige" le temps virtuel avec l'ancien multiplicateur
  // baseVirtual += (deltaReal * oldMult)
  uint64_t delta = nowReal - baseRealUs;
  baseVirtualUs += delta * (uint64_t)timeMult;

  // On rebase le point d’ancrage réel
  baseRealUs = nowReal;

  // On applique le nouveau multiplicateur
  timeMult = newMult;
}

static timestamp_t hal_get_timestamp(void)
{
  uint64_t nowReal = (uint64_t)esp_timer_get_time();
  uint64_t delta = nowReal - baseRealUs;
  uint64_t virt = baseVirtualUs + delta * (uint64_t)timeMult;
  return (timestamp_t)virt;
}

static void hal_sleep_until(timestamp_t ts)
{
  int64_t remaining = (int64_t)ts - (int64_t)hal_get_timestamp();
  if (remaining <= 0)
    return;

  // Si on a beaucoup de marge, on utilise delay() pour éviter de bloquer en busy-wait
  if (remaining >= 2000)
  {
    uint32_t ms = (uint32_t)(remaining / 1000);
    if (ms > 0)
      delay(ms);
  }

  // Finition fine
  remaining = (int64_t)ts - (int64_t)hal_get_timestamp();
  if (remaining > 0)
  {
    delayMicroseconds((uint32_t)remaining);
  }
}

// ---- Vidéo : déléguée au service ----
static void hal_update_screen(void)
{
  video.updateScreen();
}

static void hal_set_lcd_matrix(u8_t x, u8_t y, bool_t val)
{
  video.setLcdMatrix(x, y, val);
}

static void hal_set_lcd_icon(u8_t icon, bool_t val)
{
  video.setLcdIcon(icon, val);
}

static void hal_set_frequency(u32_t freq)
{
  current_freq = (uint16_t)freq;
}

static void hal_play_frequency(bool_t en)
{
  if (en)
    buzzer_play(current_freq);
  else
    buzzer_stop();
}

// ---- Input / handler ----
static int hal_handler(void)
{
  // Met à jour l'input + map L/OK/R -> hw_set_button()
  input.update();

  // Tap detection sur bouton vitesse
  static uint8_t lastDown = 0;
  uint16_t x = 0, y = 0;
  uint8_t down = 0;

  if (input.getLastTouch(x, y, down))
  {
    // front montant
    if (down && !lastDown)
    {
      bool inSpeed = video.isInsideSpeedButton(x, y);

      if (inSpeed)
      {
        uint8_t next =
            (timeMult == 1) ? 2 : (timeMult == 2) ? 4
                              : (timeMult == 4)   ? 8
                                                  : 1;

        set_time_mult(next);

        Serial.printf("[Time] Speed x%d\n", timeMult);
      }
    }
    lastDown = down;
  }

  // Log "edge" sur held (pas de spam)
  static uint8_t lastHeld = 0;
  uint8_t held = input.getHeld();
  if (held != lastHeld)
  {
    lastHeld = held;
    if (held == 1)
      Serial.println("[Input] HELD LEFT");
    else if (held == 2)
      Serial.println("[Input] HELD OK");
    else if (held == 3)
      Serial.println("[Input] HELD RIGHT");
    else
      Serial.println("[Input] HELD NONE");
  }

  return 0;
}

static hal_t hal = {
    .halt = &hal_halt,
    .log = &hal_log,
    .sleep_until = &hal_sleep_until,
    .get_timestamp = &hal_get_timestamp,
    .update_screen = &hal_update_screen,
    .set_lcd_matrix = &hal_set_lcd_matrix,
    .set_lcd_icon = &hal_set_lcd_icon,
    .set_frequency = &hal_set_frequency,
    .play_frequency = &hal_play_frequency,
    .handler = &hal_handler,
};

void setup()
{
  Serial.begin(115200);
  delay(200);

  // Init touch + hw
  input.begin();
  hw_init();

  // Brancher InputService dans VideoService pour la barre de boutons
  video.setInputService(&input);

  // Tout ce qui touche l'écran passe par VideoService
  video.initDisplay();

  baseRealUs = (uint64_t)esp_timer_get_time();
  baseVirtualUs = 0;
  timeMult = 1;

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

  // Enregistre HAL et démarre tamalib
  tamalib_register_hal(&hal);
  tamalib_set_framerate(TAMA_DISPLAY_FRAMERATE);
  tamalib_init(1000000);

  Serial.println("[Espgotchi] Step Refactoring Service started.");
}

void loop()
{
  tamalib_mainloop_step_by_step();

  // Log léger pour confirmer que la boucle vit
  static uint32_t last = 0;
  if (millis() - last > 2000)
  {
    last = millis();
    Serial.println("[Espgotchi] mainloop alive.");
  }
}
