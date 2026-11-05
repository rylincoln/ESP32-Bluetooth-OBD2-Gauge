#include "shutdown_watch.h"
#include "test.h"

namespace {
const float kOffVolts = 12.5f;

// Feeds in count low readings and says whether any of them asked for a shutdown.
bool feedLow(AutoOffWatch& watch, int count) {
  bool fired = false;
  for (int i = 0; i < count; i++) fired = watch.update(12.0f, kOffVolts) || fired;
  return fired;
}

bool feedHot(OverheatWatch& watch, int count) {
  bool fired = false;
  for (int i = 0; i < count; i++) fired = watch.update(70.0f) || fired;
  return fired;
}

// How many of count passes had a temperature read due.
int readsDue(OverheatWatch& watch, int count, bool connected) {
  int due = 0;
  for (int i = 0; i < count; i++) due += watch.readDue(connected) ? 1 : 0;
  return due;
}
}  // namespace

TEST(auto_off_waits_for_more_than_twenty_low_readings) {
  AutoOffWatch watch;
  CHECK(!feedLow(watch, 20));
  CHECK(watch.update(12.0f, kOffVolts));
}

TEST(auto_off_stays_quiet_while_volts_are_above_the_limit) {
  AutoOffWatch watch;
  for (int i = 0; i < 100; i++) CHECK(!watch.update(14.2f, kOffVolts));
}

TEST(auto_off_counts_a_reading_exactly_at_the_limit) {
  AutoOffWatch watch;
  for (int i = 0; i < 20; i++) watch.update(kOffVolts, kOffVolts);
  CHECK(watch.update(kOffVolts, kOffVolts));
}

TEST(auto_off_counts_zero_volts) {
  // A non-reply decodes to 0 V. That is how a car that has gone quiet shuts the gauge
  // down, so this has to keep counting.
  AutoOffWatch watch;
  for (int i = 0; i < 20; i++) watch.update(0.0f, kOffVolts);
  CHECK(watch.update(0.0f, kOffVolts));
}

TEST(auto_off_starts_over_after_one_good_reading) {
  AutoOffWatch watch;
  feedLow(watch, 20);
  CHECK(!watch.update(14.2f, kOffVolts));
  CHECK(!feedLow(watch, 20));
  CHECK(watch.update(12.0f, kOffVolts));
}

TEST(auto_off_uses_the_limit_it_is_given) {
  AutoOffWatch watch;
  for (int i = 0; i < 21; i++) CHECK(!watch.update(13.0f, 12.5f));
  for (int i = 0; i < 20; i++) CHECK(!watch.update(13.0f, 13.5f));
  CHECK(watch.update(13.0f, 13.5f));
}

TEST(auto_off_reset_forgets_the_count) {
  AutoOffWatch watch;
  feedLow(watch, 20);
  watch.reset();
  CHECK(!feedLow(watch, 20));
  CHECK(watch.update(12.0f, kOffVolts));
}

TEST(overheat_read_is_due_every_fiftieth_pass) {
  OverheatWatch watch;
  CHECK_EQ(readsDue(watch, 49, true), 0);
  CHECK(watch.readDue(true));
  CHECK_EQ(readsDue(watch, 49, true), 0);
  CHECK(watch.readDue(true));
}

TEST(overheat_read_never_comes_due_before_connecting) {
  // The sensor reads nonsense until the radio is active.
  OverheatWatch watch;
  CHECK_EQ(readsDue(watch, 200, false), 0);
}

TEST(overheat_read_is_due_at_once_when_a_connection_arrives_late) {
  OverheatWatch watch;
  readsDue(watch, 60, false);
  CHECK(watch.readDue(true));
}

TEST(overheat_waits_for_more_than_ten_hot_readings) {
  OverheatWatch watch;
  CHECK(!feedHot(watch, 10));
  CHECK(watch.update(70.0f));
}

TEST(overheat_counts_a_reading_exactly_at_the_limit) {
  OverheatWatch watch;
  for (int i = 0; i < 10; i++) watch.update(60.0f);
  CHECK(watch.update(60.0f));
}

TEST(overheat_stays_quiet_just_below_the_limit) {
  OverheatWatch watch;
  for (int i = 0; i < 100; i++) CHECK(!watch.update(59.9f));
}

TEST(overheat_starts_over_after_one_cool_reading) {
  OverheatWatch watch;
  feedHot(watch, 10);
  CHECK(!watch.update(40.0f));
  CHECK(!feedHot(watch, 10));
  CHECK(watch.update(70.0f));
}

TEST_MAIN()
