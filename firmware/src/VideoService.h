#pragma once

#include <Arduino.h>
#include <TFT_eSPI.h>

extern "C" {
#include "arduinogotchi_core/hw.h"
}

class InputService;

class VideoService {
public:
  void init();
  void setInputService(InputService *inputSvc);

  void setLcdMatrix(uint8_t x, uint8_t y, bool value);
  void setLcdIcon(uint8_t icon, bool value);
  void setTimeMultiplier(uint8_t mult);
  void updateScreen();

  bool isInSpeedButton(uint16_t x, uint16_t y) const;
  void refreshHeldState();

private:
  static constexpr int SCREEN_W = 320;
  static constexpr int SCREEN_H = 240;
  static constexpr int TOP_BAR_H = 28;
  static constexpr int BOTTOM_BAR_H = 50;
  static constexpr uint32_t RENDER_FPS = 12;
  static constexpr int SPEED_BTN_W = 64;
  static constexpr int SPEED_BTN_H = TOP_BAR_H;
  static constexpr int SPEED_BTN_X = SCREEN_W - SPEED_BTN_W;
  static constexpr int SPEED_BTN_Y = 0;

  bool matrixBuffer[LCD_HEIGHT][LCD_WIDTH / 8] = {{0}};
  bool iconBuffer[ICON_NUM] = {0};
  uint32_t lastMatrixHash = 0;
  uint64_t lastRenderRealUs = 0;
  uint8_t timeMultiplier = 1;
  TFT_eSPI tft = TFT_eSPI();
  InputService *input = nullptr;
  uint8_t lastHeld = 255;

  bool matrixDirty = true;
  bool topBarDirty = true;
  bool speedButtonDirty = true;
  bool buttonsDirty = true;

  uint32_t hashMatrix() const;
  void renderMatrix();
  void renderMenuTopBar();
  void renderTouchButtonsBar();
  void renderSpeedButton();
};
