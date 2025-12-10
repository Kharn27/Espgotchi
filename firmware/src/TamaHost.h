#pragma once
#include <Arduino.h>
#include <TFT_eSPI.h>

extern "C" {
#include "arduinogotchi_core/hal.h"
#include "arduinogotchi_core/tamalib.h"
}

#include "AudioService.h"
#include "VideoService.h"
#include "InputService.h"

class TamaHost {
public:
  TamaHost(AudioService &audio,
           VideoService &video,
           InputService &input,
           bool_t (&matrix)[LCD_HEIGHT][LCD_WIDTH / 8],
           bool_t (&icons)[ICON_NUM]);

  void begin();
  void loopStep();

  uint8_t getTimeMultiplier() const { return timeMult_; }

private:
  static TamaHost *instance_;

  AudioService &audio_;
  VideoService &video_;
  InputService &input_;

  bool_t (&matrix_)[LCD_HEIGHT][LCD_WIDTH / 8];
  bool_t (&icons_)[ICON_NUM];

  hal_t hal_{};

  uint8_t timeMult_ = 1;
  uint64_t baseRealUs_ = 0;
  uint64_t baseVirtualUs_ = 0;
  uint16_t currentFreq_ = 0;
  uint8_t lastLoggedHeld_ = 0;

  static void hal_halt();
  static void hal_log(log_level_t level, char *buff, ...);
  static void hal_sleep_until(timestamp_t ts);
  static timestamp_t hal_get_timestamp();
  static void hal_update_screen();
  static void hal_set_lcd_matrix(u8_t x, u8_t y, bool_t val);
  static void hal_set_lcd_icon(u8_t icon, bool_t val);
  static void hal_set_frequency(u32_t freq);
  static void hal_play_frequency(bool_t en);
  static int hal_handler();

  void setTimeMultiplier(uint8_t newMult);
  void logHeldChange(uint8_t held);
};
