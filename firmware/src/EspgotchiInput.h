#pragma once
#include <Arduino.h>

enum class VButton : uint8_t
{
  NONE = 0,
  LEFT,
  OK,
  RIGHT,
  LR
};

struct EspgotchiInputState
{
  VButton lastRead = VButton::NONE;
  VButton stable = VButton::NONE;
  uint32_t lastChangeMs = 0;
  bool fired = false;
};

class EspgotchiInput
{
public:
  void begin();
  void update();

  // “Pressed once” (évènement)
  bool consumeLeft();
  bool consumeOk();
  bool consumeRight();

  VButton consumeAny();
  VButton peekHeld() const { return held; }

  // “Held” (état)
  bool isLeftHeld() const { return held == VButton::LEFT; }
  bool isOkHeld() const { return held == VButton::OK; }
  bool isRightHeld() const { return held == VButton::RIGHT; }
  bool getLastTouch(uint16_t &x, uint16_t &y, bool &down) const
  {
    x = lastX;
    y = lastY;
    down = lastDown;
    return lastHasXY;
  }

private:
  uint16_t lastX = 0;
  uint16_t lastY = 0;
  bool lastDown = false;
  bool lastHasXY = false;
  EspgotchiInputState db;
  VButton held = VButton::NONE;

  bool readStablePress(VButton &outPressed);
};
