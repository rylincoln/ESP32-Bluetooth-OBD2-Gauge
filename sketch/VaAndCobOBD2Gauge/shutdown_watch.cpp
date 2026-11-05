#include "shutdown_watch.h"

namespace {
const uint8_t kAutoOffReadings = 20;
const uint8_t kPassesPerRead = 50;
const uint8_t kOverheatReadings = 10;
}  // namespace

bool AutoOffWatch::update(float volts, float offVolts) {
  if (volts <= offVolts) lowCount_++;
  else lowCount_ = 0;
  return lowCount_ > kAutoOffReadings;
}

void AutoOffWatch::reset() {
  lowCount_ = 0;
}

bool OverheatWatch::readDue(bool connected) {
  passes_++;
  if (passes_ < kPassesPerRead || !connected) return false;
  passes_ = 0;
  return true;
}

bool OverheatWatch::update(float cpuC) {
  if (cpuC >= kOverheatC) hotCount_++;
  else hotCount_ = 0;
  return hotCount_ > kOverheatReadings;
}
