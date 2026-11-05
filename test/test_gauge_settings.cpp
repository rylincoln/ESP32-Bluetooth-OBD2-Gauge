#include <string.h>

#include <map>
#include <string>
#include <vector>

#include "gauge_settings.h"
#include "test.h"

namespace {

// Keeps each type apart, like NVS: a value saved as one type can't be read as another.
class FakeStore : public SettingsStore {
 public:
  std::string opened;
  int writes = 0;
  std::map<std::string, uint16_t> ushorts;
  std::map<std::string, bool> bools;
  std::map<std::string, int32_t> ints;
  std::map<std::string, float> floats;
  std::map<std::string, std::string> strings;
  std::map<std::string, std::vector<uint8_t>> bytes;

  void open(const char* ns) override { opened = ns; }

  uint16_t getUShort(const char* key, uint16_t fallback) override {
    auto found = ushorts.find(key);
    return found == ushorts.end() ? fallback : found->second;
  }
  void putUShort(const char* key, uint16_t value) override {
    ushorts[key] = value;
    writes++;
  }
  bool getBool(const char* key, bool fallback) override {
    auto found = bools.find(key);
    return found == bools.end() ? fallback : found->second;
  }
  void putBool(const char* key, bool value) override {
    bools[key] = value;
    writes++;
  }
  int32_t getInt(const char* key, int32_t fallback) override {
    auto found = ints.find(key);
    return found == ints.end() ? fallback : found->second;
  }
  void putInt(const char* key, int32_t value) override {
    ints[key] = value;
    writes++;
  }
  float getFloat(const char* key, float fallback) override {
    auto found = floats.find(key);
    return found == floats.end() ? fallback : found->second;
  }
  void putFloat(const char* key, float value) override {
    floats[key] = value;
    writes++;
  }
  bool getString(const char* key, char* out, size_t size) override {
    auto found = strings.find(key);
    if (found == strings.end() || found->second.size() >= size) return false;
    strcpy(out, found->second.c_str());
    return true;
  }
  void putString(const char* key, const char* value) override {
    strings[key] = value;
    writes++;
  }
  bool getBytes(const char* key, uint8_t* out, size_t size) override {
    auto found = bytes.find(key);
    if (found == bytes.end() || found->second.size() > size) return false;
    memcpy(out, found->second.data(), found->second.size());
    return true;
  }
  void putBytes(const char* key, const uint8_t* value, size_t size) override {
    bytes[key] = std::vector<uint8_t>(value, value + size);
    writes++;
  }
};

// Written out rather than read from pidTable: gauges in cars already hold these keys,
// so changing one in the table would lose a saved value.
#ifdef FORD_T5
const char* const kWarningKeys[7] = {"ENG Load", "ECT", "MAP", "ENG SPD", "PCM Volt", "IAT", "TFT"};
#else
const char* const kWarningKeys[7] = {"ENG Load", "ECT", "MAP", "ENG SPD", "PCM Volt", "Oil Temp", "Trans Temp"};
#endif
const int kDefaultWarnings[7] = {80, 99, 35, 4000, 15, 99, 99};

}  // namespace

TEST(empty_store_gives_the_factory_defaults) {
  FakeStore store;
  GaugeSettings settings(store);
  settings.load();
  CHECK_EQ(settings.layout, 0);
  CHECK(!settings.showSystem);
  CHECK_EQ(settings.tempOffset, -50);
  CHECK_NEAR(settings.ecuOffVolt, 12.5, 0.001);
  for (int i = 0; i < 7; i++) CHECK_EQ(settings.warning[i], kDefaultWarnings[i]);
  for (int i = 0; i < 6; i++) CHECK_EQ(settings.recentClient[i], 0);
}

TEST(load_opens_the_setting_namespace) {
  FakeStore store;
  GaugeSettings settings(store);
  settings.load();
  CHECK_STR(store.opened.c_str(), "setting");
}

TEST(load_reads_what_earlier_firmware_saved) {
  // The keys and types here are what gauges already in cars have in flash.
  FakeStore store;
  store.ushorts["layout"] = 5;
  store.bools["showsystem"] = true;
  store.ints["tempOffset"] = -73;
  store.floats["ecu_off_volt"] = 13.2f;
  store.bytes["recent_client"] = {0x00, 0x1D, 0xA5, 0x68, 0x98, 0x8B};
  for (int i = 0; i < 7; i++) store.strings[kWarningKeys[i]] = std::to_string(100 + i);
  store.strings[kWarningKeys[1]] = "-5";

  GaugeSettings settings(store);
  settings.load();

  CHECK_EQ(settings.layout, 5);
  CHECK(settings.showSystem);
  CHECK_EQ(settings.tempOffset, -73);
  CHECK_NEAR(settings.ecuOffVolt, 13.2, 0.001);
  const uint8_t expected[6] = {0x00, 0x1D, 0xA5, 0x68, 0x98, 0x8B};
  CHECK(memcmp(settings.recentClient, expected, 6) == 0);
  CHECK_EQ(settings.warning[0], 100);
  CHECK_EQ(settings.warning[1], -5);
  CHECK_EQ(settings.warning[6], 106);
}

TEST(loading_writes_nothing) {
  FakeStore store;
  GaugeSettings settings(store);
  settings.load();
  CHECK_EQ(store.writes, 0);
}

TEST(load_replaces_a_layout_beyond_the_last) {
  FakeStore store;
  store.ushorts["layout"] = 9;
  GaugeSettings settings(store);
  settings.load();
  CHECK_EQ(settings.layout, 0);
}

TEST(load_replaces_a_layout_one_past_the_last) {
  FakeStore store;
  store.ushorts["layout"] = 8;
  GaugeSettings settings(store);
  settings.load();
  CHECK_EQ(settings.layout, 0);
}

TEST(load_checks_the_whole_saved_layout) {
  // 257 is stored in 16 bits and would look like layout 1 if cut down to a byte first.
  FakeStore store;
  store.ushorts["layout"] = 257;
  GaugeSettings settings(store);
  settings.load();
  CHECK_EQ(settings.layout, 0);
}

TEST(load_keeps_the_last_layout) {
  FakeStore store;
  store.ushorts["layout"] = 7;
  GaugeSettings settings(store);
  settings.load();
  CHECK_EQ(settings.layout, 7);
}

TEST(save_layout_writes_a_changed_layout) {
  FakeStore store;
  GaugeSettings settings(store);
  settings.load();
  settings.layout = 3;
  settings.saveLayout();
  CHECK_EQ(store.ushorts["layout"], 3);
  CHECK_EQ(store.writes, 1);
}

TEST(save_layout_skips_an_unchanged_layout) {
  FakeStore store;
  store.ushorts["layout"] = 3;
  GaugeSettings settings(store);
  settings.load();
  settings.saveLayout();
  CHECK_EQ(store.writes, 0);
}

TEST(save_layout_treats_nothing_saved_as_layout_zero) {
  FakeStore store;
  GaugeSettings settings(store);
  settings.load();
  settings.saveLayout();
  CHECK_EQ(store.writes, 0);
}

TEST(save_show_system_writes_only_a_change) {
  FakeStore store;
  GaugeSettings settings(store);
  settings.load();
  settings.saveShowSystem();
  CHECK_EQ(store.writes, 0);
  settings.showSystem = true;
  settings.saveShowSystem();
  CHECK(store.bools["showsystem"]);
  CHECK_EQ(store.writes, 1);
  settings.saveShowSystem();
  CHECK_EQ(store.writes, 1);
}

TEST(save_warnings_stores_the_offset_and_decimal_strings) {
  FakeStore store;
  GaugeSettings settings(store);
  settings.load();
  settings.tempOffset = -30;
  settings.warning[0] = 85;
  settings.warning[1] = -5;
  settings.saveWarnings();
  CHECK_EQ(store.ints["tempOffset"], -30);
  CHECK_STR(store.strings[kWarningKeys[0]].c_str(), "85");
  CHECK_STR(store.strings[kWarningKeys[1]].c_str(), "-5");
  CHECK_STR(store.strings[kWarningKeys[3]].c_str(), "4000");
  CHECK_EQ(store.strings.size(), 7u);
  CHECK_EQ(store.writes, 8);
}

TEST(warnings_come_back_after_saving_and_loading) {
  FakeStore store;
  GaugeSettings first(store);
  first.load();
  first.tempOffset = -12;
  for (int i = 0; i < 7; i++) first.warning[i] = 200 + i;
  first.saveWarnings();

  GaugeSettings second(store);
  second.load();
  CHECK_EQ(second.tempOffset, -12);
  for (int i = 0; i < 7; i++) CHECK_EQ(second.warning[i], 200 + i);
}

TEST(restoring_default_warnings_saves_nothing_by_itself) {
  // The menu's DEFAULT button only takes effect in flash when the screen exits.
  FakeStore store;
  store.ints["tempOffset"] = -20;
  store.strings[kWarningKeys[3]] = "3000";
  GaugeSettings settings(store);
  settings.load();
  settings.restoreDefaultWarnings();
  CHECK_EQ(settings.tempOffset, -50);
  for (int i = 0; i < 7; i++) CHECK_EQ(settings.warning[i], kDefaultWarnings[i]);
  CHECK_EQ(store.writes, 0);
  CHECK_EQ(store.ints["tempOffset"], -20);
}

TEST(save_ecu_off_volt_stores_a_float) {
  FakeStore store;
  GaugeSettings settings(store);
  settings.load();
  settings.ecuOffVolt = 13.7f;
  settings.saveEcuOffVolt();
  CHECK_NEAR(store.floats["ecu_off_volt"], 13.7, 0.001);
  CHECK_EQ(store.writes, 1);
}

TEST(restoring_the_default_ecu_off_volt_saves_nothing_by_itself) {
  FakeStore store;
  store.floats["ecu_off_volt"] = 13.7f;
  GaugeSettings settings(store);
  settings.load();
  settings.restoreDefaultEcuOffVolt();
  CHECK_NEAR(settings.ecuOffVolt, 12.5, 0.001);
  CHECK_EQ(store.writes, 0);
}

TEST(save_recent_client_writes_a_new_address_once) {
  FakeStore store;
  GaugeSettings settings(store);
  settings.load();
  const uint8_t address[6] = {0x00, 0x1D, 0xA5, 0x68, 0x98, 0x8B};
  CHECK(settings.saveRecentClient(address));
  std::vector<uint8_t> expected(address, address + 6);
  CHECK(store.bytes["recent_client"] == expected);
  CHECK(memcmp(settings.recentClient, address, 6) == 0);
  CHECK_EQ(store.writes, 1);
  CHECK(!settings.saveRecentClient(address));
  CHECK_EQ(store.writes, 1);
}

TEST(save_recent_client_skips_the_address_already_saved) {
  FakeStore store;
  store.bytes["recent_client"] = {0x00, 0x1D, 0xA5, 0x68, 0x98, 0x8B};
  GaugeSettings settings(store);
  settings.load();
  const uint8_t same[6] = {0x00, 0x1D, 0xA5, 0x68, 0x98, 0x8B};
  CHECK(!settings.saveRecentClient(same));
  CHECK_EQ(store.writes, 0);
}

TEST(save_recent_client_notices_a_difference_in_the_last_byte) {
  FakeStore store;
  store.bytes["recent_client"] = {0x00, 0x1D, 0xA5, 0x68, 0x98, 0x8B};
  GaugeSettings settings(store);
  settings.load();
  const uint8_t other[6] = {0x00, 0x1D, 0xA5, 0x68, 0x98, 0x8C};
  settings.saveRecentClient(other);
  CHECK_EQ(store.writes, 1);
}

TEST_MAIN()
