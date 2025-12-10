#pragma once

#include <Arduino.h>

class AudioService {
public:
  void init();
  void setFrequency(uint32_t freq);
  void playCurrentFrequency();
  void stop();
  uint16_t currentFrequency() const { return currentFreq; }

private:
  static constexpr uint8_t BUZZER_PIN = 26;
  static constexpr uint8_t BUZZER_CHANNEL = 0;
  uint16_t currentFreq = 0;
};
