#include <Arduino.h>
#include <TFT_eSPI.h>

extern "C" {
#include "arduinogotchi_core/hw.h"
#include "arduinogotchi_core/hal.h"
#include "esp_timer.h"
}

#include "AudioService.h"
#include "InputService.h"
#include "TamaHost.h"
#include "VideoService.h"

static TFT_eSPI tft = TFT_eSPI();
static bool_t matrix_buffer[LCD_HEIGHT][LCD_WIDTH / 8] = {{0}};
static bool_t icon_buffer[ICON_NUM] = {0};

static const uint8_t BUZZER_PIN = 26;
static const uint8_t BUZZER_CH = 0;

static AudioService audioService(BUZZER_PIN, BUZZER_CH);
static InputService inputService;
static VideoService videoService(tft, matrix_buffer, icon_buffer, inputService);
static TamaHost host(audioService, videoService, inputService, matrix_buffer, icon_buffer);

void setup()
{
  Serial.begin(115200);
  delay(200);

  inputService.begin();
  hw_init();

  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, TFT_BACKLIGHT_ON);

  audioService.begin();
  videoService.begin();
  videoService.setTimeMultiplierProvider([]() { return host.getTimeMultiplier(); });

  audioService.setFrequency(880);
  audioService.play();
  delay(120);
  audioService.stop();
  delay(80);
  audioService.setFrequency(1320);
  audioService.play();
  delay(120);
  audioService.stop();

  host.begin();
  Serial.println("[Espgotchi] Services started.");
}

void loop()
{
  host.loopStep();
}
