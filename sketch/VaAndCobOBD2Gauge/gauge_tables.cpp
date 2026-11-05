#include "gauge_tables.h"

//#define FORD_T5   // FOR FORD T5 uncomment this line

const PidInfo pidTable[kPidCount] = {
  // label, unit, code, formula, min, max, skip, decimal, warning, NVS key, menu label, menu unit, warning step
  {"ENG Load", "%", "0104", Formula::Percent, 0, 100, 0, false, 80, "ENG Load", "- Engine Load", " %  ", 5},
  {"ECT", "`C", "0105", Formula::Celsius, 0, 120, 3, false, 99, "ECT", "- Coolant Temperature", " `c  ", 1},
  {"MAP", "psi", "010B", Formula::Psi, 0, 40, 0, true, 35, "MAP", "- Manifold Air Pressure", " psi ", 5},
  {"ENG SPD", "rpm", "010C", Formula::Rpm, 0, 5000, 0, false, 4000, "ENG SPD", "- Engine Speed", " rpm ", 500},
  {"PCM Volt", "volt", "0142", Formula::Volts, 0, 16, 1, true, 15, "PCM Volt", "- PCM Voltage", " volt", 1},
#ifdef FORD_T5
  {"IAT", "`C", "010F", Formula::Celsius, 0, 120, 3, false, 99, "IAT", "- Engine Oil Temperature", " `c  ", 1},
  {"TFT", "`C", "221674", Formula::TransT5, 0, 120, 3, false, 99, "TFT", "- Transmission Fluid Temperature", " `c  ", 1},
#else
  {"Oil Temp", "`C", "015C", Formula::Celsius, 0, 120, 3, false, 99, "Oil Temp", "- Engine Oil Temperature", " `c  ", 1},
  {"Trans Temp", "`C", "221E1C", Formula::TransT6, 0, 120, 3, true, 99, "Trans Temp", "- Transmission Fluid Temperature", " `c  ", 1},
#endif
};

namespace {
const uint8_t ENG_LOAD = 0;
const uint8_t COOLANT = 1;
const uint8_t MAP = 2;
const uint8_t ENG_SPEED = 3;
const uint8_t PCM_VOLTS = kPcmVoltsPid;
const uint8_t OIL = 5;  // intake air on Ford T5
const uint8_t TRANS = 6;

const MeterType ARC = MeterType::Arc;
const MeterType NUM = MeterType::Numeric;
const MeterType BAR = MeterType::VBar;
const MeterType NONE = MeterType::None;
}  // namespace

// Each row lists a layout's slots in polling order. Slots without a meter are still
// polled, and auto-off relies on the last slot of every layout holding PCM volts.
const SlotPlan layoutTable[kLayoutCount][kSlotCount] = {
  /* layout 0   arc 1-3, numeric 7-9 */
  {{ENG_LOAD, ARC, 1}, {MAP, ARC, 2}, {ENG_SPEED, ARC, 3}, {COOLANT, NUM, 7}, {OIL, NUM, 8}, {TRANS, NUM, 9}, {PCM_VOLTS, NONE, 0}},
  /* layout 1   arc 1-3, arc 7-9 */
  {{ENG_LOAD, ARC, 1}, {MAP, ARC, 2}, {ENG_SPEED, ARC, 3}, {COOLANT, ARC, 7}, {OIL, ARC, 8}, {TRANS, ARC, 9}, {PCM_VOLTS, NONE, 0}},
  /* layout 2   numeric 1-3, numeric 7-9 */
  {{ENG_LOAD, NUM, 1}, {MAP, NUM, 2}, {ENG_SPEED, NUM, 3}, {COOLANT, NUM, 7}, {OIL, NUM, 8}, {TRANS, NUM, 9}, {PCM_VOLTS, NONE, 0}},
  /* layout 3   numeric 1-3, bars 3-4 */
  {{COOLANT, NUM, 1}, {OIL, NUM, 2}, {TRANS, NUM, 3}, {ENG_SPEED, BAR, 3}, {MAP, BAR, 4}, {PCM_VOLTS, NONE, 0}, {PCM_VOLTS, NONE, 0}},
  /* layout 4   bar 1, numeric 4-6, bar 4 */
  {{MAP, BAR, 1}, {COOLANT, NUM, 4}, {OIL, NUM, 5}, {TRANS, NUM, 6}, {PCM_VOLTS, BAR, 4}, {ENG_LOAD, NONE, 0}, {PCM_VOLTS, NONE, 0}},
  /* layout 5   bars 1-2, numeric 7-9 */
  {{MAP, BAR, 1}, {ENG_LOAD, BAR, 2}, {COOLANT, NUM, 7}, {OIL, NUM, 8}, {TRANS, NUM, 9}, {PCM_VOLTS, NONE, 0}, {PCM_VOLTS, NONE, 0}},
  /* layout 6   bars 1-4 */
  {{COOLANT, BAR, 1}, {OIL, BAR, 2}, {TRANS, BAR, 3}, {PCM_VOLTS, BAR, 4}, {ENG_LOAD, NONE, 0}, {ENG_SPEED, NONE, 0}, {PCM_VOLTS, NONE, 0}},
  /* layout 7   bars 1-4 */
  {{ENG_SPEED, BAR, 1}, {MAP, BAR, 2}, {ENG_LOAD, BAR, 3}, {PCM_VOLTS, BAR, 4}, {COOLANT, NONE, 0}, {OIL, NONE, 0}, {PCM_VOLTS, NONE, 0}},
};

/* Numeric and arc cells          Bar cells
   1 , 4 , 7   each 160x80        1 , 2 , 3 , 4   each 80x240
   2 , 5 , 8   overlapping the
   3 , 6 , 9   next column */
CellRect cellRect(MeterType meter, uint8_t cell) {
  CellRect rect = {0, 0, 0, 0};
  if (cell < 1) return rect;
  uint8_t index = cell - 1;
  if ((meter == MeterType::Numeric || meter == MeterType::Arc) && cell <= 9) {
    rect.x = (index / 3) * 80;
    rect.y = (index % 3) * 80;
    rect.width = 160;
    rect.height = 80;
  } else if (meter == MeterType::VBar && cell <= 4) {
    rect.x = index * 80;
    rect.width = 80;
    rect.height = 240;
  }
  return rect;
}

bool countsTowardAutoOff(uint8_t layout, uint8_t slot) {
  const SlotPlan& plan = layoutTable[layout][slot];
  return plan.meter == MeterType::None && plan.pid == kPcmVoltsPid;
}
