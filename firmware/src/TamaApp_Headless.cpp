#include <Arduino.h>

extern "C" {
#include "arduinogotchi_core/tamalib.h"
#include "arduinogotchi_core/hw.h"
#include "arduinogotchi_core/cpu.h"
#include "arduinogotchi_core/hal.h"
}

#include "AudioService.h"
#include "InputService.h"
#include "TamaHost.h"
#include "VideoService.h"

/**** Tama Setting ****/
#define TAMA_DISPLAY_FRAMERATE 3
/**********************/

static AudioService audioService;
static InputService inputService;
static VideoService videoService;
static TamaHost tamaHost(audioService, videoService, inputService);

void setup()
{
  Serial.begin(115200);
  delay(200);

  inputService.begin();
  videoService.setInputService(&inputService);
  videoService.init();
  audioService.init();
  tamaHost.begin();

  tamalib_register_hal(tamaHost.hal());
  tamalib_set_framerate(TAMA_DISPLAY_FRAMERATE);
  tamalib_init(1000000);

  Serial.println("[Espgotchi] Headless app started.");
}

void loop()
{
  tamalib_mainloop_step_by_step();

  static uint32_t last = 0;
  if (millis() - last > 2000)
  {
    last = millis();
    Serial.println("[Espgotchi] mainloop alive.");
  }
}
