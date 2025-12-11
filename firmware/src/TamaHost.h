#pragma once

#include <Arduino.h>

extern "C"
{
#include "arduinogotchi_core/tamalib.h"
#include "arduinogotchi_core/espgotchi_tamalib_ext.h"
#include "arduinogotchi_core/hal.h"
#include "arduinogotchi_core/cpu.h"      // <-- ajouter ça
}

class VideoService;
class InputService;

// Hôte TamaLIB : gère le temps virtuel, le HAL et le handler()
class TamaHost
{
public:
  TamaHost(VideoService &video, InputService &input);

  // displayFramerate = framerate logique (ex: 3), startTimestampUs = 1000000
  void begin(uint8_t displayFramerate, uint32_t startTimestampUs);

  // À appeler dans loop()
  void loopOnce();

private:
  VideoService &_video;
  InputService &_input;

  uint64_t _baseRealUs = 0;
  uint64_t _baseVirtualUs = 0;

  uint32_t _lastAliveLogMs = 0;

  // état handler
  uint8_t _lastTouchDown = 0;
  uint8_t _lastHeldLogged = 0;

  u32_t _tamaTsFreq;               // fréquence de référence passée à tamalib_init_* (ex: 1_000_000 pour us)
  timestamp_t _lastScreenUpdateTs; // dernier timestamp où l’on a rafraîchi l’écran

  // time scaling
  void setTimeMult(uint8_t newMult);
  timestamp_t getTimestamp();
  void sleepUntil(timestamp_t ts);

  // HAL instance -> méthodes
  void handleUpdateScreen();
  void handleSetLcdMatrix(u8_t x, u8_t y, bool_t val);
  void handleSetLcdIcon(u8_t icon, bool_t val);
  void handleSetFrequency(u32_t freq);
  void handlePlayFrequency(bool_t en);
  int handleHandler();

  // glue statique
  static TamaHost *s_instance;
  static hal_t s_hal;

  // nouveaux pour aligner le HAL avec TamaLIB
  static void *hal_malloc(u32_t size);
  static void hal_free(void *ptr);
  static bool_t hal_is_log_enabled(log_level_t level);

  static void hal_halt();
  static void hal_log(log_level_t level, char *buff, ...);
  static timestamp_t hal_get_timestamp();
  static void hal_sleep_until(timestamp_t ts);
  static void hal_update_screen();
  static void hal_set_lcd_matrix(u8_t x, u8_t y, bool_t val);
  static void hal_set_lcd_icon(u8_t icon, bool_t val);
  static void hal_set_frequency(u32_t freq);
  static void hal_play_frequency(bool_t en);
  static int hal_handler();
};
