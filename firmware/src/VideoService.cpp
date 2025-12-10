#include "VideoService.h"

extern "C" {
#include "arduinogotchi_core/bitmaps.h"
}

#include <algorithm>

VideoService::VideoService(TFT_eSPI &tft,
                           bool_t (&matrix)[LCD_HEIGHT][LCD_WIDTH / 8],
                           bool_t (&icons)[ICON_NUM],
                           InputService &input)
    : tft_(tft), matrix_(matrix), icons_(icons), input_(input) {}

void VideoService::begin() {
  tft_.init();
  tft_.setRotation(1);
  tft_.fillScreen(TFT_BLACK);
  tft_.setTextColor(TFT_GREEN, TFT_BLACK);
  tft_.setTextSize(1);
  tft_.setCursor(10, 35);
  tft_.println("Kharn27 EspGotchi");
  delay(2500);
  tft_.fillScreen(TFT_BLACK);
}

void VideoService::setTimeMultiplierProvider(const std::function<uint8_t()> &provider) {
  timeMultProvider_ = provider;
}

void VideoService::markMatrixDirty() { matrixDirty_ = true; }
void VideoService::markIconDirty() { iconsDirty_ = true; }
void VideoService::markSpeedDirty() { speedDirty_ = true; }

void VideoService::update(uint64_t nowRealUs) {
  const uint32_t interval = 1000000UL / RENDER_FPS;
  if (nowRealUs - lastRenderRealUs_ < interval) return;

  lastRenderRealUs_ = nowRealUs;

  if (matrixDirty_) {
    renderMatrixToTft();
    matrixDirty_ = false;
  }

  if (iconsDirty_) {
    renderMenuBitmapsTopbar();
    iconsDirty_ = false;
  }

  if (speedDirty_) {
    renderSpeedButtonTopbar();
    speedDirty_ = false;
  }

  uint8_t held = input_.held();
  if (held != lastHeld_) {
    lastHeld_ = held;
    renderTouchButtonsBar();
  }
}

uint32_t VideoService::hashMatrix() const {
  uint32_t h = 2166136261u;
  for (int y = 0; y < LCD_HEIGHT; y++) {
    for (int b = 0; b < LCD_WIDTH / 8; b++) {
      h ^= static_cast<uint8_t>(matrix_[y][b]);
      h *= 16777619u;
    }
  }
  return h;
}

void VideoService::renderMatrixToTft() {
  uint32_t h = hashMatrix();
  if (h == lastMatrixHash_) return;
  lastMatrixHash_ = h;

  const int margin = 8;

  int availX = margin;
  int availY = TOP_BAR_H + margin;
  int availW = SCREEN_W - 2 * margin;
  int availH = SCREEN_H - TOP_BAR_H - BOTTOM_BAR_H - 2 * margin;

  int scaleX = availW / LCD_WIDTH;
  int scaleY = availH / LCD_HEIGHT;
  int scale = std::min(scaleX, scaleY);
  if (scale < 1) scale = 1;

  int drawW = LCD_WIDTH * scale;
  int drawH = LCD_HEIGHT * scale;

  int offX = availX + (availW - drawW) / 2;
  int offY = availY + (availH - drawH) / 2;

  tft_.fillRect(offX - 2, offY - 2, drawW + 4, drawH + 4, TFT_BLACK);

  for (int y = 0; y < LCD_HEIGHT; y++) {
    for (int x = 0; x < LCD_WIDTH; x++) {
      uint8_t mask = 0b10000000 >> (x % 8);
      bool on = (matrix_[y][x / 8] & mask) != 0;
      if (on) {
        int px = offX + x * scale;
        int py = offY + y * scale;
        tft_.fillRect(px, py, scale, scale, TFT_WHITE);
      }
    }
  }
}

static void drawMonoBitmap16x9(TFT_eSPI &tft, int x, int y, const uint8_t *data, int scale = 2) {
  const int w = 16;
  const int h = 9;

  for (int j = 0; j < h; j++) {
    for (int i = 0; i < w; i++) {
      int bitIndex = j * w + i;
      int byteIndex = bitIndex / 8;
      int bitInByte = (bitIndex % 8);

      bool on = (data[byteIndex] >> bitInByte) & 0x01;
      if (on) {
        tft.fillRect(x + i * scale, y + j * scale, scale, scale, TFT_WHITE);
      }
    }
  }
}

void VideoService::renderMenuBitmapsTopbar() {
  const int barH = TOP_BAR_H;
  const int barY = 0;

  const int count = 8;
  const int iconW = 16;
  const int iconH = 9;
  const int scale = 2;

  int iconsAreaW = SCREEN_W - SPEED_BTN_W;
  int slotW = iconsAreaW / count;

  int drawW = iconW * scale;
  int drawH = iconH * scale;

  int y = barY + (barH - drawH) / 2;

  tft_.fillRect(0, barY, SCREEN_W, barH, TFT_BLACK);

  for (int i = 0; i < count; i++) {
    if (icons_[i]) {
      int slotX = i * slotW;
      tft_.fillRect(slotX, barY, slotW, barH, TFT_DARKGREY);
    }
  }

  for (int i = 0; i < count; i++) {
    int centerX = i * slotW + slotW / 2;
    int x = centerX - drawW / 2;
    const uint8_t *icon = bitmaps + (i * 18);
    drawMonoBitmap16x9(tft_, x, y, icon, scale);
  }

  tft_.drawFastHLine(0, barH - 1, SCREEN_W, TFT_DARKGREY);
}

void VideoService::renderTouchButtonsBar() {
  const int barH = BOTTOM_BAR_H;
  const int barY = SCREEN_H - barH;
  const int count = 3;
  const int slotW = SCREEN_W / count;

  tft_.fillRect(0, barY, SCREEN_W, barH, TFT_BLACK);

  for (int i = 0; i < count; i++) {
    int x = i * slotW;

    bool isActive =
        (lastHeld_ == 1 && i == 0) ||
        (lastHeld_ == 2 && i == 1) ||
        (lastHeld_ == 3 && i == 2);

    uint16_t fill = isActive ? TFT_DARKGREY : TFT_BLACK;

    tft_.fillRect(x + 6, barY + 6, slotW - 12, barH - 12, fill);
    tft_.drawRect(x + 6, barY + 6, slotW - 12, barH - 12, TFT_DARKGREY);

    tft_.setTextColor(TFT_WHITE, fill);
    tft_.setTextSize(2);

    const char *label = (i == 0) ? "L" : (i == 1) ? "OK" : "R";

    int tx = x + slotW / 2 - ((i == 1) ? 12 : 6);
    int ty = barY + barH / 2 - 8;

    tft_.setCursor(tx, ty);
    tft_.print(label);
  }

  tft_.drawFastHLine(0, barY, SCREEN_W, TFT_DARKGREY);
}

void VideoService::renderSpeedButtonTopbar() {
  uint16_t bg = TFT_BLACK;
  tft_.fillRect(SPEED_BTN_X, SPEED_BTN_Y, SPEED_BTN_W, SPEED_BTN_H, bg);
  tft_.drawRect(SPEED_BTN_X + 2, SPEED_BTN_Y + 4, SPEED_BTN_W - 4, SPEED_BTN_H - 8, TFT_DARKGREY);

  tft_.setTextSize(1);
  tft_.setTextColor(TFT_WHITE, bg);

  int tx = SPEED_BTN_X + 10;
  int ty = SPEED_BTN_Y + 9;

  tft_.setCursor(tx, ty);
  tft_.print("SPD x");
  if (timeMultProvider_) {
    tft_.print(timeMultProvider_());
  } else {
    tft_.print('?');
  }
}
