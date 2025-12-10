#include "TamaHost.h"

#include "AudioService.h"
#include "InputService.h"
#include "VideoService.h"
#include <esp_timer.h>

extern "C" {
#include "arduinogotchi_core/hw.h"
}

TamaHost *TamaHost::instance = nullptr;

TamaHost::TamaHost(AudioService &audioSvc, VideoService &videoSvc,
                   InputService &inputSvc)
    : audio(audioSvc), video(videoSvc), input(inputSvc) {
  instance = this;

  halStruct.halt = &TamaHost::halt;
  halStruct.log = &TamaHost::log;
  halStruct.sleep_until = &TamaHost::sleepUntil;
  halStruct.get_timestamp = &TamaHost::getTimestamp;
  halStruct.update_screen = &TamaHost::updateScreen;
  halStruct.set_lcd_matrix = &TamaHost::setLcdMatrix;
  halStruct.set_lcd_icon = &TamaHost::setLcdIcon;
  halStruct.set_frequency = &TamaHost::setFrequency;
  halStruct.play_frequency = &TamaHost::playFrequency;
  halStruct.handler = &TamaHost::handler;
}

void TamaHost::begin() {
  baseRealUs = static_cast<uint64_t>(esp_timer_get_time());
  baseVirtualUs = 0;
  timeMult = 1;
  video.setTimeMultiplier(timeMult);
}

hal_t *TamaHost::hal() { return &halStruct; }

void TamaHost::setTimeMultiplier(uint8_t newMult) {
  uint64_t nowReal = static_cast<uint64_t>(esp_timer_get_time());
  uint64_t delta = nowReal - baseRealUs;
  baseVirtualUs += delta * static_cast<uint64_t>(timeMult);
  baseRealUs = nowReal;
  timeMult = newMult;
  video.setTimeMultiplier(timeMult);
}

void TamaHost::halt() {}

void TamaHost::log(log_level_t level, char *buff, ...) {
  (void)level;
  Serial.println(buff);
}

timestamp_t TamaHost::getTimestamp() {
  uint64_t nowReal = static_cast<uint64_t>(esp_timer_get_time());
  uint64_t delta = nowReal - instance->baseRealUs;
  uint64_t virt = instance->baseVirtualUs + delta * static_cast<uint64_t>(instance->timeMult);
  return static_cast<timestamp_t>(virt);
}

void TamaHost::sleepUntil(timestamp_t ts) {
  int64_t remaining = static_cast<int64_t>(ts) -
                      static_cast<int64_t>(TamaHost::getTimestamp());
  if (remaining <= 0)
    return;

  if (remaining >= 2000) {
    uint32_t ms = static_cast<uint32_t>(remaining / 1000);
    if (ms > 0)
      delay(ms);
  }

  remaining = static_cast<int64_t>(ts) -
              static_cast<int64_t>(TamaHost::getTimestamp());
  if (remaining > 0) {
    delayMicroseconds(static_cast<uint32_t>(remaining));
  }
}

void TamaHost::updateScreen() { instance->video.updateScreen(); }

void TamaHost::setLcdMatrix(u8_t x, u8_t y, bool_t val) {
  instance->video.setLcdMatrix(x, y, val);
}

void TamaHost::setLcdIcon(u8_t icon, bool_t val) {
  instance->video.setLcdIcon(icon, val);
}

void TamaHost::setFrequency(u32_t freq) {
  instance->audio.setFrequency(freq);
}

void TamaHost::playFrequency(bool_t en) {
  if (en)
    instance->audio.playCurrentFrequency();
  else
    instance->audio.stop();
}

int TamaHost::handler() {
  instance->onHandler();
  return 0;
}

void TamaHost::onHandler() {
  bool heldChanged = input.updateButtons();
  if (heldChanged) {
    video.refreshHeldState();
  }

  uint16_t x = 0;
  uint16_t y = 0;
  if (input.consumeTouchDown(x, y)) {
    if (video.isInSpeedButton(x, y)) {
      uint8_t next = (timeMult == 1) ? 2 : (timeMult == 2) ? 4 :
                     (timeMult == 4) ? 8 : 1;
      setTimeMultiplier(next);
      Serial.printf("[Time] Speed x%d\n", timeMult);
    }
  }
}
