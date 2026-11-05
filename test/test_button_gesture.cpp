#include "button_gesture.h"
#include "test.h"

TEST(button_left_alone_does_nothing) {
  ButtonGesture button;
  CHECK(button.update(false, 0) == ButtonEvent::None);
  CHECK(button.update(false, 5000) == ButtonEvent::None);
}

TEST(first_sample_of_a_press_does_nothing_yet) {
  ButtonGesture button;
  CHECK(button.update(true, 1000) == ButtonEvent::None);
}

TEST(press_seen_on_the_next_debounced_pass_is_a_hold) {
  // Callers wait 200 ms or more between passes while the button is down, which is
  // what gives a hold its feel of about 200 ms.
  ButtonGesture button;
  button.update(true, 1000);
  CHECK(button.update(true, 1200) == ButtonEvent::Hold);
}

TEST(press_released_before_the_next_pass_is_a_tap) {
  ButtonGesture button;
  button.update(true, 1000);
  CHECK(button.update(false, 1200) == ButtonEvent::Tap);
}

TEST(a_tap_is_reported_once) {
  ButtonGesture button;
  button.update(true, 1000);
  button.update(false, 1200);
  CHECK(button.update(false, 1400) == ButtonEvent::None);
}

TEST(press_is_not_a_hold_before_the_threshold) {
  ButtonGesture button;
  button.update(true, 1000);
  CHECK(button.update(true, 1000 + ButtonGesture::kHoldMs - 1) == ButtonEvent::None);
  CHECK(button.update(true, 1000 + ButtonGesture::kHoldMs) == ButtonEvent::Hold);
}

TEST(hold_threshold_stays_below_the_debounce_delay) {
  // Otherwise a hold would take one pass longer and every menu would feel different.
  CHECK(ButtonGesture::kHoldMs < 200);
}

TEST(releasing_after_a_hold_is_not_a_tap) {
  ButtonGesture button;
  button.update(true, 1000);
  button.update(true, 1200);
  CHECK(button.update(false, 1400) == ButtonEvent::None);
}

TEST(keeping_the_button_down_after_a_hold_starts_a_new_press) {
  ButtonGesture button;
  button.update(true, 1000);
  button.update(true, 1200);
  CHECK(button.update(true, 1400) == ButtonEvent::None);
  CHECK(button.update(true, 1600) == ButtonEvent::Hold);
}

TEST(hold_works_across_the_millis_rollover) {
  ButtonGesture button;
  button.update(true, 0xFFFFFFF0u);
  CHECK(button.update(true, 0xFFFFFFF0u + 200u) == ButtonEvent::Hold);
}

TEST(press_seen_only_on_separate_samples_still_counts) {
  // The Bluetooth scan blocks for seconds and only feeds in samples where the button
  // is down, so a press carries over until the next one it sees.
  ButtonGesture button;
  button.update(true, 1000);
  CHECK(button.update(true, 7000) == ButtonEvent::Hold);
}

TEST_MAIN()
