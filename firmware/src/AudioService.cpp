#include "AudioService.h"

AudioService::AudioService(uint8_t buzzerPin, uint8_t channel)
    : buzzerPin_(buzzerPin), channel_(channel) {}

void AudioService::begin() {
  pinMode(buzzerPin_, OUTPUT);
  ledcSetup(channel_, 2000, 10);
  ledcAttachPin(buzzerPin_, channel_);
  stop();
}

void AudioService::setFrequency(uint32_t freq) {
  currentFreq_ = static_cast<uint16_t>(freq);
  applyTone();
}

void AudioService::play() { applyTone(); }

void AudioService::stop() { ledcWrite(channel_, 0); }

void AudioService::setMuted(bool muted) {
  muted_ = muted;
  applyTone();
}

void AudioService::setVolume(uint16_t dutyCycle) {
  volume_ = dutyCycle;
  if (volume_ > 1023) volume_ = 1023;
  applyTone();
}

void AudioService::applyTone() {
  if (muted_ || currentFreq_ == 0) {
    stop();
    return;
  }

  ledcWriteTone(channel_, currentFreq_);
  ledcWrite(channel_, volume_);
}
