#include "button_gesture.h"

ButtonEvent ButtonGesture::update(bool isDown, uint32_t nowMs) {
  if (!isDown) {
    if (!pressed_) return ButtonEvent::None;
    pressed_ = false;
    return ButtonEvent::Tap;
  }
  if (!pressed_) {
    pressed_ = true;
    pressedAt_ = nowMs;
    return ButtonEvent::None;
  }
  if (nowMs - pressedAt_ < kHoldMs) return ButtonEvent::None;
  pressed_ = false;
  return ButtonEvent::Hold;
}
