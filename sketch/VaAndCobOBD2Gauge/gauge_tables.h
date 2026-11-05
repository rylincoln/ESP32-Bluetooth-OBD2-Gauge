// The gauge's PIDs and layouts, each described once. Drawing, polling, the warning
// screen and saved settings all read these tables, so changing a layout or adding a
// PID means editing gauge_tables.cpp and nothing else.
#pragma once

#include <stdint.h>

#include "obd_reply.h"

const uint8_t kPidCount = 7;
const uint8_t kLayoutCount = 8;
const uint8_t kSlotCount = 7;
const uint8_t kPcmVoltsPid = 4;

struct PidInfo {
  const char* label;
  const char* unit;
  const char* code;
  Formula formula;
  int16_t minValue;
  int16_t maxValue;
  uint8_t skip;  // rounds to skip between polls, 0-3
  bool showDecimal;
  int16_t defaultWarning;
  const char* settingKey;  // saved warning value's NVS key: renaming it loses saved values
  const char* menuLabel;   // on the warning screen
  const char* menuUnit;
  int16_t warningStep;  // how far one press moves the warning value
};

enum class MeterType : uint8_t {
  None,  // polled, but not drawn
  Numeric,
  Arc,
  VBar,
};

struct SlotPlan {
  uint8_t pid;  // PID index
  MeterType meter;
  uint8_t cell;  // 1-9 for numeric and arc meters, 1-4 for vertical bars
};

extern const PidInfo pidTable[kPidCount];
extern const SlotPlan layoutTable[kLayoutCount][kSlotCount];

struct CellRect {
  uint16_t x;
  uint16_t y;
  uint16_t width;
  uint16_t height;
};

// Where a cell sits on the 320x240 screen. A cell outside the grid has no area.
CellRect cellRect(MeterType meter, uint8_t cell);

// True when this slot's reading feeds the auto-off count.
bool countsTowardAutoOff(uint8_t layout, uint8_t slot);
