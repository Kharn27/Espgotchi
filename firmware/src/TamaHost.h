#pragma once

#include <Arduino.h>

extern "C" {
#include "arduinogotchi_core/hal.h"
}

class AudioService;
class VideoService;
class InputService;

class TamaHost {
public:
  TamaHost(AudioService &audioSvc, VideoService &videoSvc, InputService &inputSvc);

  void begin();
  hal_t *hal();

  void setTimeMultiplier(uint8_t newMult);
  uint8_t getTimeMultiplier() const { return timeMult; }

private:
  static TamaHost *instance;

  AudioService &audio;
  VideoService &video;
  InputService &input;

  hal_t halStruct{};
  uint8_t timeMult = 1;
  uint64_t baseRealUs = 0;
  uint64_t baseVirtualUs = 0;

  static void halt();
  static void log(log_level_t level, char *buff, ...);
  static void sleepUntil(timestamp_t ts);
  static timestamp_t getTimestamp();
  static void updateScreen();
  static void setLcdMatrix(u8_t x, u8_t y, bool_t val);
  static void setLcdIcon(u8_t icon, bool_t val);
  static void setFrequency(u32_t freq);
  static void playFrequency(bool_t en);
  static int handler();

  void onHandler();
};
