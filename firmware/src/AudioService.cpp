#include "AudioService.h"

// Config hardware buzzer (esp32-cyd)
static const int BUZZER_PIN = 26;
static const int BUZZER_CH = 0;
static const int BUZZER_BASE_FREQ = 2000;
static const int BUZZER_RES_BITS = 10; // 10 bits -> 0..1023 duty

void AudioService::begin() {
  if (_initialized) return;

  pinMode(BUZZER_PIN, OUTPUT);
  ledcSetup(BUZZER_CH, BUZZER_BASE_FREQ, BUZZER_RES_BITS);
  ledcAttachPin(BUZZER_PIN, BUZZER_CH);
  ledcWrite(BUZZER_CH, 0);

  _initialized = true;
}

void AudioService::setFrequency(uint32_t freqHz) {
  _currentFreq = freqHz;
  if (_isPlaying) {
    applyTone();
  }
}

void AudioService::play() {
  _isPlaying = true;
  applyTone();
}

void AudioService::stop() {
  _isPlaying = false;
  if (!_initialized) return;
  ledcWrite(BUZZER_CH, 0);
}

void AudioService::setMuted(bool muted) {
  _muted = muted;
  applyTone();
}

void AudioService::setVolume(uint8_t volume) {
  _volume = volume;
  applyTone();
}

void AudioService::applyTone() {
  if (!_initialized) return;

  // Cas "silence" : pas de duty
  if (!_isPlaying || _muted || _currentFreq == 0 || _volume == 0) {
    ledcWrite(BUZZER_CH, 0);
    return;
  }

  // Fréquence
  ledcWriteTone(BUZZER_CH, _currentFreq);

  // Volume : map 0–255 -> 0..(2^RES-1)
  uint32_t maxDuty = (1u << BUZZER_RES_BITS) - 1u; // 1023
  uint32_t duty = (static_cast<uint32_t>(_volume) * maxDuty) / 255u;
  ledcWrite(BUZZER_CH, duty);
}
