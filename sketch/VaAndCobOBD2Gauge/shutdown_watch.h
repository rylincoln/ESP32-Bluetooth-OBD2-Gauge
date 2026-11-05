// Decides when the gauge should put itself to sleep. Each watch counts bad readings in
// a row, so one odd value can't shut the gauge down. shutdown.h does the shutting down.
#pragma once

#include <stdint.h>

class AutoOffWatch {
 public:
  // Feed it each PCM volts reading. True once more than 20 in a row have been at or
  // below offVolts. A non-reply arrives here as 0 V and counts.
  bool update(float volts, float offVolts);

  // Starts the count over, for when the readings stop being comparable.
  void reset();

 private:
  uint8_t lowCount_ = 0;
};

class OverheatWatch {
 public:
  static const uint8_t kOverheatC = 60;

  // Call it once per pass of any loop. True when it's time to read the CPU temperature,
  // every 50th pass and only once connected, because the sensor needs the radio active.
  bool readDue(bool connected);

  // Feed it each reading. True once more than 10 in a row have been at or above
  // kOverheatC.
  bool update(float cpuC);

 private:
  uint8_t passes_ = 0;
  uint8_t hotCount_ = 0;
};
