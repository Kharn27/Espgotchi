#pragma once

#include <Arduino.h>

// Service audio ESP32 CYD (buzzer via LEDC)
class AudioService {
public:
  // Init hardware (LEDC + pin)
  void begin();

  // API bas niveau (utilisée par le HAL)
  void setFrequency(uint32_t freqHz);
  void play();
  void stop();

  // Options "qualité de vie"
  void setMuted(bool muted);
  bool isMuted() const { return _muted; }

  // Volume 0–255 (255 = max, 0 = silence)
  void setVolume(uint8_t volume);
  uint8_t volume() const { return _volume; }

private:
  uint32_t _currentFreq = 0;
  bool _isPlaying = false;
  bool _muted = false;
  uint8_t _volume = 255; // 0–255
  bool _initialized = false;

  void applyTone();
};
