#include "VideoService.h"
#include "InputService.h"
#include "esp_timer.h"

extern "C"
{
#include "arduinogotchi_core/bitmaps.h"
}

// Palette LCD : fond gris clair, segments foncés, cadre sombre
static constexpr uint16_t LCD_COLOR_BG = TFT_LIGHTGREY;
static constexpr uint16_t LCD_COLOR_PIXEL = TFT_BLACK;
static constexpr uint16_t LCD_COLOR_FRAME = TFT_DARKGREY;

// timeMult est défini dans TamaApp
extern uint8_t timeMult;

VideoService::VideoService()
    : _tft()
{
}

void VideoService::initDisplay()
{
  // Init driver TFT
  _tft.init();
  _tft.setRotation(1);
  _tft.fillScreen(TFT_BLACK);

  // Backlight si dispo
#ifdef TFT_BL
  pinMode(TFT_BL, OUTPUT);
#ifdef TFT_BACKLIGHT_ON
  digitalWrite(TFT_BL, TFT_BACKLIGHT_ON);
#else
  digitalWrite(TFT_BL, HIGH);
#endif
#endif
}

void VideoService::begin()
{
  memset(_matrix, 0, sizeof(_matrix));
  memset(_icons, 0, sizeof(_icons));
  _lastRenderRealUs = 0;
  _lastMatrixHash = 0;
}

void VideoService::clearScreen()
{
  _tft.fillScreen(TFT_BLACK);
}

void VideoService::showSplash(const char *text)
{
  _tft.setTextColor(TFT_GREEN, TFT_BLACK);
  _tft.setTextSize(1);
  _tft.setCursor(10, 35);
  _tft.println(text);
}

void VideoService::setLcdMatrix(u8_t x, u8_t y, bool_t val)
{
  uint8_t mask;
  if (val)
  {
    mask = 0b10000000 >> (x % 8);
    _matrix[y][x / 8] = _matrix[y][x / 8] | mask;
  }
  else
  {
    mask = 0b01111111;
    for (byte i = 0; i < (x % 8); i++)
    {
      mask = (mask >> 1) | 0b10000000;
    }
    _matrix[y][x / 8] = _matrix[y][x / 8] & mask;
  }
}

void VideoService::setLcdIcon(u8_t icon, bool_t val)
{
  if (icon < ICON_NUM)
  {
    _icons[icon] = val;
  }
}

void VideoService::setInputService(InputService *input)
{
  _input = input;
}

uint32_t VideoService::hashMatrix() const
{
  uint32_t h = 2166136261u;
  for (int y = 0; y < LCD_HEIGHT; y++)
  {
    for (int b = 0; b < LCD_WIDTH / 8; b++)
    {
      h ^= (uint8_t)_matrix[y][b];
      h *= 16777619u;
    }
  }
  return h;
}

void VideoService::renderMatrixToTft()
{
  uint32_t h = hashMatrix();
  if (h == _lastMatrixHash)
  {
    return;
  }
  _lastMatrixHash = h;

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

  // Cadre autour de l'écran LCD
  _tft.fillRect(offX - 2, offY - 2, drawW + 4, drawH + 4, LCD_COLOR_FRAME);

  // Fond LCD (pixels "éteints")
  _tft.fillRect(offX, offY, drawW, drawH, LCD_COLOR_BG);

  for (int y = 0; y < LCD_HEIGHT; y++)
  {
    for (int x = 0; x < LCD_WIDTH; x++)
    {
      uint8_t mask = 0b10000000 >> (x % 8);
      bool on = (_matrix[y][x / 8] & mask) != 0;
      if (on)
      {
        int px = offX + x * scale;
        int py = offY + y * scale;
        // Pixels "allumés" du LCD : segments foncés
        _tft.fillRect(px, py, scale, scale, LCD_COLOR_PIXEL);
      }
    }
  }
}

void VideoService::drawMonoBitmap16x9(int x, int y, const uint8_t *data, int scale)
{
  const int w = 16;
  const int h = 9;

  // 1) On cherche les colonnes réellement utilisées (où au moins un pixel est à 1)
  int minCol = w;
  int maxCol = -1;

  for (int j = 0; j < h; j++)
  {
    for (int i = 0; i < w; i++)
    {
      int bitIndex = j * w + i;
      int byteIndex = bitIndex / 8;
      int bitInByte = bitIndex % 8;

      bool on = (data[byteIndex] >> bitInByte) & 0x01;
      if (on)
      {
        if (i < minCol)
          minCol = i;
        if (i > maxCol)
          maxCol = i;
      }
    }
  }

  // 2) Calcul du décalage horizontal pour centrer la "vraie" largeur
  int offsetX = 0;
  if (maxCol >= minCol)
  {
    int activeWidth = maxCol - minCol + 1;
    // Décalage = centrer activeWidth dans w, puis compenser minCol
    offsetX = (w - activeWidth) / 2 - minCol;
  }

  // 3) Dessin des pixels avec ce décalage
  for (int j = 0; j < h; j++)
  {
    for (int i = 0; i < w; i++)
    {
      int bitIndex = j * w + i;
      int byteIndex = bitIndex / 8;
      int bitInByte = bitIndex % 8;

      bool on = (data[byteIndex] >> bitInByte) & 0x01;
      if (on)
      {
        int drawX = x + (i + offsetX) * scale;
        int drawY = y + j * scale;
        _tft.fillRect(drawX, drawY, scale, scale, TFT_WHITE);
      }
    }
  }
}

void VideoService::renderMenuBitmapsTopbar()
{
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

  // Clear barre
  _tft.fillRect(0, barY, SCREEN_W, barH, TFT_BLACK);

  // highlight slot sélectionné
  for (int i = 0; i < count; i++)
  {
    if (_icons[i])
    {
      int slotX = i * slotW;
      _tft.fillRect(slotX, barY, slotW, barH, TFT_DARKGREY);
    }
  }

  // icônes (boîte 16x9 centrée dans le slot)
  for (int i = 0; i < count; i++)
  {
    int centerX = i * slotW + slotW / 2;
    int x = centerX - drawW / 2;

    const uint8_t *icon = bitmaps + (i * 18);
    drawMonoBitmap16x9(x, y, icon, scale);
  }

  _tft.drawFastHLine(0, barH - 1, SCREEN_W, TFT_DARKGREY);
}

void VideoService::renderTouchButtonsBar()
{
  const int barH = BOTTOM_BAR_H;
  const int barY = SCREEN_H - barH;
  const int count = 3;
  const int slotW = SCREEN_W / count;

  // On lit l'état via InputService (si dispo)
  LogicalButton held = LogicalButton::NONE;
  if (_input)
  {
    held = _input->getHeld(); // LEFT / OK / RIGHT / NONE
  }

  // Anti-flicker simple : redraw seulement si changement
  static bool first = true;
  static LogicalButton lastHeld = LogicalButton::NONE;

  if (!first && held == lastHeld)
    return;

  first = false;
  lastHeld = held;

  _tft.fillRect(0, barY, SCREEN_W, barH, TFT_BLACK);

  for (int i = 0; i < count; i++)
  {
    int x = i * slotW;

    bool isActive =
        (held == LogicalButton::LEFT && i == 0) ||
        (held == LogicalButton::OK && i == 1) ||
        (held == LogicalButton::RIGHT && i == 2);

    uint16_t fill = isActive ? TFT_DARKGREY : TFT_BLACK;

    _tft.fillRect(x + 6, barY + 6, slotW - 12, barH - 12, fill);
    _tft.drawRect(x + 6, barY + 6, slotW - 12, barH - 12, TFT_DARKGREY);

    _tft.setTextColor(TFT_WHITE, fill);
    _tft.setTextSize(2);

    const char *label = (i == 0) ? "L" : (i == 1) ? "OK"
                                                  : "R";

    int tx = x + slotW / 2 - ((i == 1) ? 12 : 6);
    int ty = barY + barH / 2 - 8;

    _tft.setCursor(tx, ty);
    _tft.print(label);
  }

  _tft.drawFastHLine(0, barY, SCREEN_W, TFT_DARKGREY);
}

void VideoService::renderSpeedButtonTopbar()
{
  uint16_t bg = TFT_BLACK;
  _tft.fillRect(SPEED_BTN_X, SPEED_BTN_Y, SPEED_BTN_W, SPEED_BTN_H, bg);

  // Cadre qui va jusqu’à la ligne de séparation
  _tft.drawRect(SPEED_BTN_X + 2, SPEED_BTN_Y + 2,
                SPEED_BTN_W - 4, SPEED_BTN_H - 2,
                TFT_DARKGREY);

  // Texte taille 1, mais centré verticalement
  _tft.setTextSize(1);
  _tft.setTextColor(TFT_WHITE, bg);

  const int textH = 8 * 1; // hauteur d'un caractère en textSize=1

  int tx = SPEED_BTN_X + 10;                        // on garde ton offset horizontal
  int ty = SPEED_BTN_Y + (SPEED_BTN_H - textH) / 2; // centrage vertical

  _tft.setCursor(tx, ty);
  _tft.print("SPD x");
  _tft.print(timeMult);
}

void VideoService::updateScreen()
{
  uint64_t now = (uint64_t)esp_timer_get_time();
  uint32_t interval = 1000000UL / RENDER_FPS;

  if (now - _lastRenderRealUs < interval)
  {
    return;
  }
  _lastRenderRealUs = now;

  renderMenuBitmapsTopbar();
  renderSpeedButtonTopbar();
  renderMatrixToTft();
  renderTouchButtonsBar();
}

bool VideoService::isInsideSpeedButton(uint16_t x, uint16_t y) const
{
  return (x >= SPEED_BTN_X) && (x < SPEED_BTN_X + SPEED_BTN_W) &&
         (y >= SPEED_BTN_Y) && (y < SPEED_BTN_Y + SPEED_BTN_H);
}
