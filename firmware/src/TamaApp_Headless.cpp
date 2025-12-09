#include <Arduino.h>

extern "C"
{
#include "arduinogotchi_core/bitmaps.h"
#include "arduinogotchi_core/tamalib.h"
#include "arduinogotchi_core/hw.h"
#include "arduinogotchi_core/cpu.h"
#include "arduinogotchi_core/hal.h"
}

#include "EspgotchiInputC.h"
#include "EspgotchiButtons.h"
#include <TFT_eSPI.h>
#include "esp_timer.h"

/**** Tama Setting ****/
#define TAMA_DISPLAY_FRAMERATE 3
#define BUZZER_PIN 26
#define BUZZER_CH  0

/**********************/

/**** TamaLib buffers (comme dans ArduinoGotchi) ****/
static bool_t matrix_buffer[LCD_HEIGHT][LCD_WIDTH / 8] = {{0}};
static bool_t icon_buffer[ICON_NUM] = {0};
static uint16_t current_freq = 0;
/****************************************************/

static TFT_eSPI tft = TFT_eSPI();

static const int SCREEN_W = 320;
static const int SCREEN_H = 240;
static const int TOP_BAR_H = 28;
static const int BOTTOM_BAR_H = 50;    // même hauteur que tes zones tactiles
static const uint32_t RENDER_FPS = 12; // 8, 10, 12, 15 à tester
static const int SPEED_BTN_W = 64;
static const int SPEED_BTN_H = TOP_BAR_H; // 28
static const int SPEED_BTN_X = SCREEN_W - SPEED_BTN_W;
static const int SPEED_BTN_Y = 0;
static uint8_t timeMult = 1; // 1,2,4
static uint64_t baseRealUs = 0;
static uint64_t baseVirtualUs = 0;
static uint64_t lastRenderRealUs = 0;
static uint32_t lastMatrixHash = 0;


static void buzzer_init() {
  pinMode(BUZZER_PIN, OUTPUT);
  ledcSetup(BUZZER_CH, 2000, 10);
  ledcAttachPin(BUZZER_PIN, BUZZER_CH);
  ledcWrite(BUZZER_CH, 0);
}

static void buzzer_play(uint32_t freq) {
  if (freq == 0) { ledcWrite(BUZZER_CH, 0); return; }
  ledcWriteTone(BUZZER_CH, freq);
  ledcWrite(BUZZER_CH, 512);
}

static void buzzer_stop() {
  ledcWrite(BUZZER_CH, 0);
}

// ---- HAL impl minimale ----
static void hal_halt(void) {}

static void hal_log(log_level_t level, char *buff, ...)
{
  // Simple log brut
  Serial.println(buff);
}

static uint32_t hash_matrix()
{
  uint32_t h = 2166136261u;
  for (int y = 0; y < LCD_HEIGHT; y++)
  {
    for (int b = 0; b < LCD_WIDTH / 8; b++)
    {
      h ^= (uint8_t)matrix_buffer[y][b];
      h *= 16777619u;
    }
  }
  return h;
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

static void render_matrix_to_tft()
{
  uint32_t h = hash_matrix();
  if (h == lastMatrixHash)
    return;
  lastMatrixHash = h;

  // Marges internes
  const int margin = 8;

  // Zone disponible pour le LCD (on réserve le top bar)
  int availX = margin;
  int availY = TOP_BAR_H + margin;
  int availW = SCREEN_W - 2 * margin;
  int availH = SCREEN_H - TOP_BAR_H - BOTTOM_BAR_H - 2 * margin;

  // Calcul d’un scale qui rentre DANS cette zone
  int scaleX = availW / LCD_WIDTH;
  int scaleY = availH / LCD_HEIGHT;
  int scale = min(scaleX, scaleY);
  if (scale < 1)
    scale = 1;

  int drawW = LCD_WIDTH * scale;
  int drawH = LCD_HEIGHT * scale;

  // Centrage dans la zone disponible
  int offX = availX + (availW - drawW) / 2;
  int offY = availY + (availH - drawH) / 2;

  // Efface uniquement la zone LCD
  tft.fillRect(offX - 2, offY - 2, drawW + 4, drawH + 4, TFT_BLACK);

  // Dessin des pixels blancs
  for (int y = 0; y < LCD_HEIGHT; y++)
  {
    for (int x = 0; x < LCD_WIDTH; x++)
    {
      uint8_t mask = 0b10000000 >> (x % 8);
      bool on = (matrix_buffer[y][x / 8] & mask) != 0;

      if (on)
      {
        int px = offX + x * scale;
        int py = offY + y * scale;
        tft.fillRect(px, py, scale, scale, TFT_WHITE);
      }
    }
  }
}

static void drawMonoBitmap16x9(int x, int y, const uint8_t *data, int scale = 2)
{
  // 16x9 pixels, 18 bytes attendus (comme ArduinoGotchi)
  const int w = 16;
  const int h = 9;

  for (int j = 0; j < h; j++)
  {
    for (int i = 0; i < w; i++)
    {
      int bitIndex = j * w + i;
      int byteIndex = bitIndex / 8;
      // int bitInByte = 7 - (bitIndex % 8); // MSB-first (premier essai)
      int bitInByte = (bitIndex % 8); // LSB-first

      bool on = (data[byteIndex] >> bitInByte) & 0x01;
      if (on)
      {
        tft.fillRect(x + i * scale, y + j * scale, scale, scale, TFT_WHITE);
      }
    }
  }
}

static void render_menu_bitmaps_topbar()
{
  const int barH = TOP_BAR_H; // 28
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

  // 1) Nettoyage propre de la barre
  tft.fillRect(0, barY, SCREEN_W, barH, TFT_BLACK);

  // 2) Highlight "slot" entier pour l’icône sélectionnée
  //    (plus lisible pour le jeu)
  for (int i = 0; i < count; i++)
  {
    if (icon_buffer[i])
    {
      int slotX = i * slotW;
      // Un fond gris sur tout le slot
      tft.fillRect(slotX, barY, slotW, barH, TFT_DARKGREY);
    }
  }

  // 3) Dessin des icônes par-dessus
  for (int i = 0; i < count; i++)
  {
    int centerX = i * slotW + slotW / 2;
    int x = centerX - drawW / 2;

    const uint8_t *icon = bitmaps + (i * 18);

    drawMonoBitmap16x9(x, y, icon, scale);

    // (Optionnel) légère bordure de slot, très discrète
    // tft.drawLine(i * slotW, barY + 4, i * slotW, barY + barH - 5, TFT_BLACK);
  }

  // 4) Séparateur sous la barre
  tft.drawFastHLine(0, barH - 1, SCREEN_W, TFT_DARKGREY);
}

static void render_touch_buttons_bar()
{
  const int barH = BOTTOM_BAR_H; // 50
  const int barY = SCREEN_H - barH;
  const int count = 3;
  const int slotW = SCREEN_W / count;

  // 👇 On lit l'état tactile actuel
  uint8_t held = espgotchi_input_peek_held(); // 0 none, 1 left, 2 ok, 3 right

  // Anti-flicker simple : redraw seulement si changement
  static uint8_t lastHeld = 255;
  if (held == lastHeld)
    return;
  lastHeld = held;

  // Fond barre
  tft.fillRect(0, barY, SCREEN_W, barH, TFT_BLACK);

  // Dessin des 3 boutons
  for (int i = 0; i < count; i++)
  {
    int x = i * slotW;

    bool isActive =
        (held == 1 && i == 0) ||
        (held == 2 && i == 1) ||
        (held == 3 && i == 2);

    uint16_t fill = isActive ? TFT_DARKGREY : TFT_BLACK;

    // "carte" bouton
    tft.fillRect(x + 6, barY + 6, slotW - 12, barH - 12, fill);
    tft.drawRect(x + 6, barY + 6, slotW - 12, barH - 12, TFT_DARKGREY);

    // Label
    tft.setTextColor(TFT_WHITE, fill);
    tft.setTextSize(2);

    const char *label = (i == 0) ? "L" : (i == 1) ? "OK"
                                                  : "R";

    // Centrage approximatif simple
    int tx = x + slotW / 2 - ((i == 1) ? 12 : 6);
    int ty = barY + barH / 2 - 8;

    tft.setCursor(tx, ty);
    tft.print(label);
  }

  // Ligne de séparation au-dessus de la barre
  tft.drawFastHLine(0, barY, SCREEN_W, TFT_DARKGREY);
}

static void render_speed_button_topbar()
{
  // Fond bouton
  uint16_t bg = TFT_BLACK;
  tft.fillRect(SPEED_BTN_X, SPEED_BTN_Y, SPEED_BTN_W, SPEED_BTN_H, bg);

  // Liseré
  tft.drawRect(SPEED_BTN_X + 2, SPEED_BTN_Y + 4, SPEED_BTN_W - 4, SPEED_BTN_H - 8, TFT_DARKGREY);

  // Texte
  tft.setTextSize(1);
  tft.setTextColor(TFT_WHITE, bg);

  int tx = SPEED_BTN_X + 10;
  int ty = SPEED_BTN_Y + 9;

  tft.setCursor(tx, ty);
  tft.print("SPD x");
  tft.print(timeMult);
}

static void hal_update_screen(void)
{
  uint64_t now = (uint64_t)esp_timer_get_time();
  uint32_t interval = 1000000UL / RENDER_FPS;

  if (now - lastRenderRealUs < interval)
  {
    // On saute ce frame d’affichage
    return;
  }
  lastRenderRealUs = now;
  render_menu_bitmaps_topbar();
  render_speed_button_topbar();
  render_matrix_to_tft();
  render_touch_buttons_bar();
}

static void hal_set_lcd_matrix(u8_t x, u8_t y, bool_t val)
{
  uint8_t mask;
  if (val)
  {
    mask = 0b10000000 >> (x % 8);
    matrix_buffer[y][x / 8] = matrix_buffer[y][x / 8] | mask;
  }
  else
  {
    mask = 0b01111111;
    for (byte i = 0; i < (x % 8); i++)
    {
      mask = (mask >> 1) | 0b10000000;
    }
    matrix_buffer[y][x / 8] = matrix_buffer[y][x / 8] & mask;
  }
}

static void hal_set_lcd_icon(u8_t icon, bool_t val)
{
  icon_buffer[icon] = val;
}

static void hal_set_frequency(u32_t freq)
{
  current_freq = (uint16_t)freq;
}

static void hal_play_frequency(bool_t en)
{
  if (en) buzzer_play(current_freq);
  else buzzer_stop();
}

// C’est ICI qu’on branche ton touch -> hw_set_button()
static int hal_handler(void)
{
  espgotchi_buttons_update();

  // Tap detection sur bouton vitesse
  static uint8_t lastDown = 0;
  uint16_t x = 0, y = 0;
  uint8_t down = 0;

  if (espgotchi_input_get_last_touch(&x, &y, &down))
  {
    // front montant
    if (down && !lastDown)
    {
      bool inSpeed =
          (x >= SPEED_BTN_X) && (x < SPEED_BTN_X + SPEED_BTN_W) &&
          (y >= SPEED_BTN_Y) && (y < SPEED_BTN_Y + SPEED_BTN_H);

      if (inSpeed)
      {
        uint8_t next =
            (timeMult == 1) ? 2 : (timeMult == 2) ? 4 : (timeMult == 4) ? 8
                                                  : 1;

        set_time_mult(next);

        Serial.printf("[Time] Speed x%d\n", timeMult);
      }
    }
    lastDown = down;
  }

  // Log "edge" sur held (pas de spam)
  static uint8_t lastHeld = 0;
  uint8_t held = espgotchi_input_peek_held();
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
  espgotchi_input_begin();
  hw_init();

  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, TFT_BACKLIGHT_ON);

  tft.init();
  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);
  baseRealUs = (uint64_t)esp_timer_get_time();
  baseVirtualUs = 0;
  timeMult = 1;

  tft.setTextColor(TFT_GREEN, TFT_BLACK);
  tft.setTextSize(1);
  tft.setCursor(10, 35);
  tft.println("Kharn27 EspGotchi");
  delay(2500);
  tft.setCursor(10, 35);
  tft.println("                   ");

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

  Serial.println("[Espgotchi] Step 5A headless app started.");
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
