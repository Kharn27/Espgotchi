#include "AudioService.h"

void AudioService::init() {
  pinMode(BUZZER_PIN, OUTPUT);
  ledcSetup(BUZZER_CHANNEL, 2000, 10);
  ledcAttachPin(BUZZER_PIN, BUZZER_CHANNEL);
  ledcWrite(BUZZER_CHANNEL, 0);
}

void AudioService::setFrequency(uint32_t freq) {
  currentFreq = static_cast<uint16_t>(freq);
}

void AudioService::playCurrentFrequency() {
  if (currentFreq == 0) {
    ledcWrite(BUZZER_CHANNEL, 0);
    return;
  }
  ledcWriteTone(BUZZER_CHANNEL, currentFreq);
  ledcWrite(BUZZER_CHANNEL, 512);
}

void AudioService::stop() {
  ledcWrite(BUZZER_CHANNEL, 0);
}
