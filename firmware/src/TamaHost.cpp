#include "TamaHost.h"

#include <cstdarg>

extern "C" {
#include "arduinogotchi_core/cpu.h"
#include "arduinogotchi_core/hw.h"
#include "esp_timer.h"
}

TamaHost *TamaHost::instance_ = nullptr;

TamaHost::TamaHost(AudioService &audio,
                   VideoService &video,
                   InputService &input,
                   bool_t (&matrix)[LCD_HEIGHT][LCD_WIDTH / 8],
                   bool_t (&icons)[ICON_NUM])
    : audio_(audio), video_(video), input_(input), matrix_(matrix), icons_(icons) {
  instance_ = this;

  hal_.halt = &TamaHost::hal_halt;
  hal_.log = &TamaHost::hal_log;
  hal_.sleep_until = &TamaHost::hal_sleep_until;
  hal_.get_timestamp = &TamaHost::hal_get_timestamp;
  hal_.update_screen = &TamaHost::hal_update_screen;
  hal_.set_lcd_matrix = &TamaHost::hal_set_lcd_matrix;
  hal_.set_lcd_icon = &TamaHost::hal_set_lcd_icon;
  hal_.set_frequency = &TamaHost::hal_set_frequency;
  hal_.play_frequency = &TamaHost::hal_play_frequency;
  hal_.handler = &TamaHost::hal_handler;
}

void TamaHost::begin() {
  baseRealUs_ = (uint64_t)esp_timer_get_time();
  baseVirtualUs_ = 0;
  timeMult_ = 1;
  lastLoggedHeld_ = 0;

  tamalib_register_hal(&hal_);
  tamalib_set_framerate(3);
  tamalib_init(1000000);
}

void TamaHost::loopStep() {
  tamalib_mainloop_step_by_step();

  static uint32_t last = 0;
  if (millis() - last > 2000) {
    last = millis();
    Serial.println("[Espgotchi] mainloop alive.");
  }
}

void TamaHost::setTimeMultiplier(uint8_t newMult) {
  uint64_t nowReal = (uint64_t)esp_timer_get_time();

  uint64_t delta = nowReal - baseRealUs_;
  baseVirtualUs_ += delta * (uint64_t)timeMult_;

  baseRealUs_ = nowReal;
  timeMult_ = newMult;

  video_.markSpeedDirty();
}

void TamaHost::logHeldChange(uint8_t held) {
  if (held == lastLoggedHeld_) return;

  lastLoggedHeld_ = held;
  if (held == 1)
    Serial.println("[Input] HELD LEFT");
  else if (held == 2)
    Serial.println("[Input] HELD OK");
  else if (held == 3)
    Serial.println("[Input] HELD RIGHT");
  else
    Serial.println("[Input] HELD NONE");
}

void TamaHost::hal_halt() {}

void TamaHost::hal_log(log_level_t, char *buff, ...) {
  va_list args;
  va_start(args, buff);
  Serial.println(buff);
  va_end(args);
}

timestamp_t TamaHost::hal_get_timestamp() {
  uint64_t nowReal = (uint64_t)esp_timer_get_time();
  uint64_t delta = nowReal - instance_->baseRealUs_;
  uint64_t virt = instance_->baseVirtualUs_ + delta * (uint64_t)instance_->timeMult_;
  return (timestamp_t)virt;
}

void TamaHost::hal_sleep_until(timestamp_t ts) {
  int64_t remaining = (int64_t)ts - (int64_t)hal_get_timestamp();
  if (remaining <= 0) return;

  if (remaining >= 2000) {
    uint32_t ms = (uint32_t)(remaining / 1000);
    if (ms > 0) delay(ms);
  }

  remaining = (int64_t)ts - (int64_t)hal_get_timestamp();
  if (remaining > 0) {
    delayMicroseconds((uint32_t)remaining);
  }
}

void TamaHost::hal_update_screen() {
  instance_->video_.update((uint64_t)esp_timer_get_time());
}

void TamaHost::hal_set_lcd_matrix(u8_t x, u8_t y, bool_t val) {
  uint8_t mask;
  if (val) {
    mask = 0b10000000 >> (x % 8);
    instance_->matrix_[y][x / 8] = instance_->matrix_[y][x / 8] | mask;
  } else {
    mask = 0b01111111;
    for (byte i = 0; i < (x % 8); i++) {
      mask = (mask >> 1) | 0b10000000;
    }
    instance_->matrix_[y][x / 8] = instance_->matrix_[y][x / 8] & mask;
  }
  instance_->video_.markMatrixDirty();
}

void TamaHost::hal_set_lcd_icon(u8_t icon, bool_t val) {
  instance_->icons_[icon] = val;
  instance_->video_.markIconDirty();
}

void TamaHost::hal_set_frequency(u32_t freq) {
  instance_->currentFreq_ = (uint16_t)freq;
}

void TamaHost::hal_play_frequency(bool_t en) {
  if (en) {
    instance_->audio_.setFrequency(instance_->currentFreq_);
    instance_->audio_.play();
  } else {
    instance_->audio_.stop();
  }
}

int TamaHost::hal_handler() {
  instance_->input_.poll();

  if (instance_->input_.tapStartedIn(VideoService::SPEED_BTN_X,
                                     VideoService::SPEED_BTN_Y,
                                     VideoService::SPEED_BTN_W,
                                     VideoService::SPEED_BTN_H)) {
    uint8_t next =
        (instance_->timeMult_ == 1) ? 2 : (instance_->timeMult_ == 2) ? 4 : (instance_->timeMult_ == 4) ? 8 : 1;
    instance_->setTimeMultiplier(next);
    Serial.printf("[Time] Speed x%d\n", instance_->timeMult_);
  }

  instance_->logHeldChange(instance_->input_.held());
  return 0;
}

