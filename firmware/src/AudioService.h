#pragma once
#include <Arduino.h>

class AudioService {
public:
  AudioService(uint8_t buzzerPin, uint8_t channel);

  void begin();
  void setFrequency(uint32_t freq);
  void play();
  void stop();

  void setMuted(bool muted);
  bool isMuted() const { return muted_; }

  void setVolume(uint16_t dutyCycle);
  uint16_t volume() const { return volume_; }

private:
  uint8_t buzzerPin_;
  uint8_t channel_;
  uint16_t volume_ = 512;
  uint16_t currentFreq_ = 0;
  bool muted_ = false;

  void applyTone();
};
