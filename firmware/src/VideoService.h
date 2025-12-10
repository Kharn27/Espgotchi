#pragma once
#include <Arduino.h>
#include <TFT_eSPI.h>
#include <functional>

extern "C" {
#include "arduinogotchi_core/hal.h"
#include "arduinogotchi_core/hw.h"
}

#include "InputService.h"

class VideoService {
public:
  VideoService(TFT_eSPI &tft,
               bool_t (&matrix)[LCD_HEIGHT][LCD_WIDTH / 8],
               bool_t (&icons)[ICON_NUM],
               InputService &input);

  void begin();
  void setTimeMultiplierProvider(const std::function<uint8_t()> &provider);
  void markMatrixDirty();
  void markIconDirty();
  void markSpeedDirty();
  void update(uint64_t nowRealUs);

  static const int SCREEN_W = 320;
  static const int SCREEN_H = 240;
  static const int TOP_BAR_H = 28;
  static const int BOTTOM_BAR_H = 50;
  static const int SPEED_BTN_W = 64;
  static const int SPEED_BTN_H = TOP_BAR_H;
  static const int SPEED_BTN_X = SCREEN_W - SPEED_BTN_W;
  static const int SPEED_BTN_Y = 0;
  static const uint32_t RENDER_FPS = 12;

private:
  TFT_eSPI &tft_;
  bool_t (&matrix_)[LCD_HEIGHT][LCD_WIDTH / 8];
  bool_t (&icons_)[ICON_NUM];
  InputService &input_;
  std::function<uint8_t()> timeMultProvider_;

  uint64_t lastRenderRealUs_ = 0;
  uint32_t lastMatrixHash_ = 0;
  bool matrixDirty_ = true;
  bool iconsDirty_ = true;
  bool speedDirty_ = true;
  uint8_t lastHeld_ = 255;

  void renderMenuBitmapsTopbar();
  void renderTouchButtonsBar();
  void renderSpeedButtonTopbar();
  void renderMatrixToTft();

  uint32_t hashMatrix() const;
};
