#include "gauge_tables.h"
#include "test.h"

#include <cstring>

namespace {

bool overlap(const CellRect& a, const CellRect& b) {
  return a.x < b.x + b.width && b.x < a.x + a.width && a.y < b.y + b.height && b.y < a.y + a.height;
}

}  // namespace

// ---- layouts ----

TEST(every_slot_holds_a_known_pid) {
  for (int layout = 0; layout < kLayoutCount; layout++)
    for (int slot = 0; slot < kSlotCount; slot++)
      CHECK(layoutTable[layout][slot].pid < kPidCount);
}

TEST(last_slot_of_every_layout_polls_pcm_volts_without_a_meter) {
  for (int layout = 0; layout < kLayoutCount; layout++) {
    const SlotPlan& last = layoutTable[layout][kSlotCount - 1];
    CHECK_STR(pidTable[last.pid].code, "0142");
    CHECK(last.meter == MeterType::None);
  }
}

TEST(every_meter_sits_in_a_real_cell_on_screen) {
  for (int layout = 0; layout < kLayoutCount; layout++) {
    for (int slot = 0; slot < kSlotCount; slot++) {
      const SlotPlan& plan = layoutTable[layout][slot];
      if (plan.meter == MeterType::None) continue;
      CellRect r = cellRect(plan.meter, plan.cell);
      CHECK(r.width > 0);
      CHECK(r.height > 0);
      CHECK(r.x + r.width <= 320);
      CHECK(r.y + r.height <= 240);
    }
  }
}

TEST(meters_on_a_layout_never_overlap) {
  for (int layout = 0; layout < kLayoutCount; layout++) {
    for (int first = 0; first < kSlotCount; first++) {
      for (int second = first + 1; second < kSlotCount; second++) {
        const SlotPlan& a = layoutTable[layout][first];
        const SlotPlan& b = layoutTable[layout][second];
        if (a.meter == MeterType::None || b.meter == MeterType::None) continue;
        CHECK(!overlap(cellRect(a.meter, a.cell), cellRect(b.meter, b.cell)));
      }
    }
  }
}

TEST(cell_geometry_matches_the_screen_grid) {
  // Numeric and arc cells run down three columns; bar cells run across four.
  CellRect numeric = cellRect(MeterType::Numeric, 8);
  CHECK_EQ(numeric.x, 160);
  CHECK_EQ(numeric.y, 80);
  CHECK_EQ(numeric.width, 160);
  CHECK_EQ(numeric.height, 80);
  CellRect arc = cellRect(MeterType::Arc, 4);
  CHECK_EQ(arc.x, 80);
  CHECK_EQ(arc.y, 0);
  CellRect bar = cellRect(MeterType::VBar, 3);
  CHECK_EQ(bar.x, 160);
  CHECK_EQ(bar.y, 0);
  CHECK_EQ(bar.width, 80);
  CHECK_EQ(bar.height, 240);
}

TEST(cell_outside_the_grid_has_no_area) {
  CHECK_EQ(cellRect(MeterType::Numeric, 0).width, 0);
  CHECK_EQ(cellRect(MeterType::Numeric, 10).width, 0);
  CHECK_EQ(cellRect(MeterType::VBar, 5).width, 0);
  CHECK_EQ(cellRect(MeterType::None, 1).width, 0);
}

TEST(auto_off_counts_the_undrawn_pcm_volts_slots) {
  const int expected[kLayoutCount] = {1, 1, 1, 2, 1, 2, 1, 1};
  for (int layout = 0; layout < kLayoutCount; layout++) {
    int counted = 0;
    for (int slot = 0; slot < kSlotCount; slot++) {
      if (countsTowardAutoOff(layout, slot)) counted++;
    }
    CHECK_EQ(counted, expected[layout]);
  }
}

TEST(auto_off_ignores_a_pcm_volts_slot_that_has_a_meter) {
  // Layout 4 draws PCM volts in slot 4 and polls it again, undrawn, in slot 6.
  CHECK(!countsTowardAutoOff(4, 4));
  CHECK(countsTowardAutoOff(4, 6));
}

TEST(auto_off_ignores_undrawn_slots_holding_other_pids) {
  // Layout 6 polls engine load and speed in slots 4 and 5 without drawing them.
  CHECK(!countsTowardAutoOff(6, 4));
  CHECK(!countsTowardAutoOff(6, 5));
}

// ---- PIDs ----

TEST(pcm_volts_pid_is_the_one_auto_off_watches) {
  CHECK_STR(pidTable[kPcmVoltsPid].code, "0142");
  CHECK(pidTable[kPcmVoltsPid].formula == Formula::Volts);
}

TEST(setting_keys_fit_the_nvs_limit_and_are_unique) {
  for (int i = 0; i < kPidCount; i++) {
    CHECK(std::strlen(pidTable[i].settingKey) <= 15);
    for (int j = i + 1; j < kPidCount; j++) {
      CHECK(std::strcmp(pidTable[i].settingKey, pidTable[j].settingKey) != 0);
    }
  }
}

TEST(default_warning_values_are_inside_each_pids_range) {
  for (int i = 0; i < kPidCount; i++) {
    CHECK(pidTable[i].minValue < pidTable[i].maxValue);
    CHECK(pidTable[i].defaultWarning >= pidTable[i].minValue);
    CHECK(pidTable[i].defaultWarning <= pidTable[i].maxValue);
    CHECK(pidTable[i].warningStep > 0);
  }
}

TEST(pid_codes_are_two_or_three_bytes_of_hex) {
  for (int i = 0; i < kPidCount; i++) {
    size_t length = std::strlen(pidTable[i].code);
    CHECK(length == 4 || length == 6);
    CHECK(std::strspn(pidTable[i].code, "0123456789ABCDEF") == length);
  }
}

// Renaming one of these would orphan the warning value a gauge has already saved.
TEST(saved_warning_keys_stay_as_shipped) {
  CHECK_STR(pidTable[0].settingKey, "ENG Load");
  CHECK_STR(pidTable[1].settingKey, "ECT");
  CHECK_STR(pidTable[2].settingKey, "MAP");
  CHECK_STR(pidTable[3].settingKey, "ENG SPD");
  CHECK_STR(pidTable[4].settingKey, "PCM Volt");
#ifdef FORD_T5
  CHECK_STR(pidTable[5].settingKey, "IAT");
  CHECK_STR(pidTable[6].settingKey, "TFT");
#else
  CHECK_STR(pidTable[5].settingKey, "Oil Temp");
  CHECK_STR(pidTable[6].settingKey, "Trans Temp");
#endif
}

TEST(vehicle_variant_chooses_the_last_two_pids) {
#ifdef FORD_T5
  CHECK_STR(pidTable[5].code, "010F");
  CHECK_STR(pidTable[6].code, "221674");
  CHECK(pidTable[6].formula == Formula::TransT5);
  CHECK(!pidTable[6].showDecimal);
#else
  CHECK_STR(pidTable[5].code, "015C");
  CHECK_STR(pidTable[6].code, "221E1C");
  CHECK(pidTable[6].formula == Formula::TransT6);
  CHECK(pidTable[6].showDecimal);
#endif
}

TEST_MAIN()
