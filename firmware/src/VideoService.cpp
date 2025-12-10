#include "VideoService.h"

#include "InputService.h"
#include <esp_timer.h>

extern "C" {
#include "arduinogotchi_core/bitmaps.h"
}

uint32_t VideoService::hashMatrix() const {
  uint32_t h = 2166136261u;
  for (int y = 0; y < LCD_HEIGHT; y++) {
    for (int b = 0; b < LCD_WIDTH / 8; b++) {
      h ^= static_cast<uint8_t>(matrixBuffer[y][b]);
      h *= 16777619u;
    }
  }
  return h;
}

void VideoService::init() {
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, TFT_BACKLIGHT_ON);

  tft.init();
  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);

  matrixDirty = topBarDirty = speedButtonDirty = buttonsDirty = true;
  lastHeld = 255;
  lastMatrixHash = 0;
}

void VideoService::setInputService(InputService *inputSvc) {
  input = inputSvc;
}

void VideoService::setLcdMatrix(uint8_t x, uint8_t y, bool value) {
  uint8_t mask;
  if (value) {
    mask = 0b10000000 >> (x % 8);
    matrixBuffer[y][x / 8] = matrixBuffer[y][x / 8] | mask;
  } else {
    mask = 0b01111111;
    for (byte i = 0; i < (x % 8); i++) {
      mask = (mask >> 1) | 0b10000000;
    }
    matrixBuffer[y][x / 8] = matrixBuffer[y][x / 8] & mask;
  }
  matrixDirty = true;
}

void VideoService::setLcdIcon(uint8_t icon, bool value) {
  iconBuffer[icon] = value;
  topBarDirty = true;
}

void VideoService::setTimeMultiplier(uint8_t mult) {
  timeMultiplier = mult;
  speedButtonDirty = true;
}

bool VideoService::isInSpeedButton(uint16_t x, uint16_t y) const {
  return (x >= SPEED_BTN_X) && (x < SPEED_BTN_X + SPEED_BTN_W) &&
         (y >= SPEED_BTN_Y) && (y < SPEED_BTN_Y + SPEED_BTN_H);
}

void VideoService::renderMatrix() {
  uint32_t h = hashMatrix();
  if (!matrixDirty && h == lastMatrixHash) {
    return;
  }
  lastMatrixHash = h;
  matrixDirty = false;

  const int margin = 8;
  int availX = margin;
  int availY = TOP_BAR_H + margin;
  int availW = SCREEN_W - 2 * margin;
  int availH = SCREEN_H - TOP_BAR_H - BOTTOM_BAR_H - 2 * margin;

  int scaleX = availW / LCD_WIDTH;
  int scaleY = availH / LCD_HEIGHT;
  int scale = min(scaleX, scaleY);
  if (scale < 1)
    scale = 1;

  int drawW = LCD_WIDTH * scale;
  int drawH = LCD_HEIGHT * scale;

  int offX = availX + (availW - drawW) / 2;
  int offY = availY + (availH - drawH) / 2;

  tft.fillRect(offX - 2, offY - 2, drawW + 4, drawH + 4, TFT_BLACK);

  for (int y = 0; y < LCD_HEIGHT; y++) {
    for (int x = 0; x < LCD_WIDTH; x++) {
      uint8_t mask = 0b10000000 >> (x % 8);
      bool on = (matrixBuffer[y][x / 8] & mask) != 0;
      if (on) {
        int px = offX + x * scale;
        int py = offY + y * scale;
        tft.fillRect(px, py, scale, scale, TFT_WHITE);
      }
    }
  }
}

static void drawMonoBitmap16x9(TFT_eSPI &tft, int x, int y, const uint8_t *data,
                               int scale = 2) {
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

void VideoService::renderMenuTopBar() {
  if (!topBarDirty) {
    return;
  }
  topBarDirty = false;

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

  tft.fillRect(0, barY, SCREEN_W, barH, TFT_BLACK);

  for (int i = 0; i < count; i++) {
    if (iconBuffer[i]) {
      int slotX = i * slotW;
      tft.fillRect(slotX, barY, slotW, barH, TFT_DARKGREY);
    }
  }

  for (int i = 0; i < count; i++) {
    int centerX = i * slotW + slotW / 2;
    int x = centerX - drawW / 2;

    const uint8_t *icon = bitmaps + (i * 18);

    drawMonoBitmap16x9(tft, x, y, icon, scale);
  }

  tft.drawFastHLine(0, barH - 1, SCREEN_W, TFT_DARKGREY);
}

void VideoService::renderTouchButtonsBar() {
  if (!buttonsDirty) {
    return;
  }
  buttonsDirty = false;

  const int barH = BOTTOM_BAR_H;
  const int barY = SCREEN_H - barH;
  const int count = 3;
  const int slotW = SCREEN_W / count;

  uint8_t held = input ? input->peekHeld() : 0;

  tft.fillRect(0, barY, SCREEN_W, barH, TFT_BLACK);

  for (int i = 0; i < count; i++) {
    int x = i * slotW;

    bool isActive = (held == 1 && i == 0) || (held == 2 && i == 1) ||
                    (held == 3 && i == 2);

    uint16_t fill = isActive ? TFT_DARKGREY : TFT_BLACK;

    tft.fillRect(x + 6, barY + 6, slotW - 12, barH - 12, fill);
    tft.drawRect(x + 6, barY + 6, slotW - 12, barH - 12, TFT_DARKGREY);

    tft.setTextColor(TFT_WHITE, fill);
    tft.setTextSize(2);

    const char *label = (i == 0) ? "L" : (i == 1) ? "OK" : "R";

    int tx = x + slotW / 2 - ((i == 1) ? 12 : 6);
    int ty = barY + barH / 2 - 8;

    tft.setCursor(tx, ty);
    tft.print(label);
  }

  tft.drawFastHLine(0, barY, SCREEN_W, TFT_DARKGREY);
}

void VideoService::renderSpeedButton() {
  if (!speedButtonDirty) {
    return;
  }
  speedButtonDirty = false;

  uint16_t bg = TFT_BLACK;
  tft.fillRect(SPEED_BTN_X, SPEED_BTN_Y, SPEED_BTN_W, SPEED_BTN_H, bg);

  tft.drawRect(SPEED_BTN_X + 2, SPEED_BTN_Y + 4, SPEED_BTN_W - 4,
               SPEED_BTN_H - 8, TFT_DARKGREY);

  tft.setTextSize(1);
  tft.setTextColor(TFT_WHITE, bg);

  int tx = SPEED_BTN_X + 10;
  int ty = SPEED_BTN_Y + 9;

  tft.setCursor(tx, ty);
  tft.print("SPD x");
  tft.print(timeMultiplier);
}

void VideoService::refreshHeldState() {
  if (!input) {
    return;
  }
  uint8_t held = input->peekHeld();
  if (held != lastHeld) {
    lastHeld = held;
    buttonsDirty = true;
  }
}

void VideoService::updateScreen() {
  uint64_t now = static_cast<uint64_t>(esp_timer_get_time());
  uint32_t interval = 1000000UL / RENDER_FPS;

  if (now - lastRenderRealUs < interval) {
    return;
  }
  lastRenderRealUs = now;

  renderMenuTopBar();
  renderSpeedButton();
  renderMatrix();
  renderTouchButtonsBar();
}
