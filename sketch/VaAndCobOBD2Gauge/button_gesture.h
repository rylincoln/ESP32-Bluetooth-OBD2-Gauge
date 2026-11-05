// Turns samples of the button's state into the two gestures the gauge responds to.
#pragma once

#include <stdint.h>

enum class ButtonEvent : uint8_t {
  None,
  Tap,   // pressed and released
  Hold,  // still pressed when checked again after kHoldMs
};

class ButtonGesture {
 public:
  // How long a press must last to count as a hold. Callers sample the button once
  // per debounced pass, at least 200 ms apart while it's down, so anything under 200
  // means "still down on the next pass". That makes a real hold about 200 ms.
  static const uint32_t kHoldMs = 100;

  // Feed this one sample per pass. After a hold, keeping the button down counts as a
  // new press.
  ButtonEvent update(bool isDown, uint32_t nowMs);

 private:
  bool pressed_ = false;
  uint32_t pressedAt_ = 0;
};
