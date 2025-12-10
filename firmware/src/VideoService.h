#pragma once

#include <Arduino.h>
#include <TFT_eSPI.h>

extern "C"
{
#include "arduinogotchi_core/tamalib.h"
#include "arduinogotchi_core/hw.h"
#include "arduinogotchi_core/hal.h"
#include "arduinogotchi_core/cpu.h"
}

class InputService;

class VideoService
{
public:
  explicit VideoService();

  // Init hardware / TFT
  void initDisplay();

  // Déjà existant : reset des buffers internes
  void begin();

  // Helpers “app”
  void clearScreen();
  void showSplash(const char *text);

  // Brancher l'input
  void setInputService(InputService *input);

  // Hooks HAL
  void setLcdMatrix(u8_t x, u8_t y, bool_t val);
  void setLcdIcon(u8_t icon, bool_t val);
  void updateScreen();

  // Utilitaire pour TamaHost / handler() : hit test bouton SPD
  bool isInsideSpeedButton(uint16_t x, uint16_t y) const;

private:
  TFT_eSPI _tft; // propriété du service

  InputService *_input = nullptr;

  // Layout / constantes écran
  static constexpr int SCREEN_W = 320;
  static constexpr int SCREEN_H = 240;
  static constexpr int TOP_BAR_H = 28;
  static constexpr int BOTTOM_BAR_H = 50;
  static constexpr uint32_t RENDER_FPS = 12;

  static constexpr int SPEED_BTN_W = 64;
  static constexpr int SPEED_BTN_H = TOP_BAR_H;
  static constexpr int SPEED_BTN_X = SCREEN_W - SPEED_BTN_W;
  static constexpr int SPEED_BTN_Y = 0;

  // Buffers internes (équivalents aux statiques actuels)
  bool_t _matrix[LCD_HEIGHT][LCD_WIDTH / 8];
  bool_t _icons[ICON_NUM];

  uint64_t _lastRenderRealUs = 0;
  uint32_t _lastMatrixHash = 0;

  // Helpers internes
  uint32_t hashMatrix() const;
  void renderMatrixToTft();
  void renderMenuBitmapsTopbar();
  void renderTouchButtonsBar();
  void renderSpeedButtonTopbar();
  void drawMonoBitmap16x9(int x, int y, const uint8_t *data, int scale = 2);
};
