#include "gauge_settings.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

namespace {
const char* const kNamespace = "setting";
const char* const kLayoutKey = "layout";
const char* const kShowSystemKey = "showsystem";
const char* const kTempOffsetKey = "tempOffset";
const char* const kEcuOffVoltKey = "ecu_off_volt";
const char* const kRecentClientKey = "recent_client";

const int8_t kFactoryTempOffset = -50;
const float kFactoryEcuOffVolt = 12.5f;
const size_t kWarningTextSize = 16;
}  // namespace

void GaugeSettings::load() {
  store_.open(kNamespace);
  layout = store_.getUShort(kLayoutKey, 0);
  if (layout > kLayoutCount) layout = 0;
  showSystem = store_.getBool(kShowSystemKey, false);
  tempOffset = store_.getInt(kTempOffsetKey, kFactoryTempOffset);
  for (uint8_t i = 0; i < kPidCount; i++) {
    char text[kWarningTextSize];
    bool saved = store_.getString(pidTable[i].settingKey, text, sizeof(text));
    warning[i] = saved ? atoi(text) : pidTable[i].defaultWarning;
  }
  store_.getBytes(kRecentClientKey, recentClient, sizeof(recentClient));
  ecuOffVolt = store_.getFloat(kEcuOffVoltKey, kFactoryEcuOffVolt);
}

void GaugeSettings::saveLayout() {
  if (store_.getUShort(kLayoutKey, 0) != layout) store_.putUShort(kLayoutKey, layout);
}

void GaugeSettings::saveShowSystem() {
  if (store_.getBool(kShowSystemKey, false) != showSystem) store_.putBool(kShowSystemKey, showSystem);
}

void GaugeSettings::saveWarnings() {
  store_.putInt(kTempOffsetKey, tempOffset);
  for (uint8_t i = 0; i < kPidCount; i++) {
    char text[kWarningTextSize];
    snprintf(text, sizeof(text), "%d", warning[i]);
    store_.putString(pidTable[i].settingKey, text);
  }
}

void GaugeSettings::saveEcuOffVolt() {
  store_.putFloat(kEcuOffVoltKey, ecuOffVolt);
}

bool GaugeSettings::saveRecentClient(const uint8_t* address) {
  if (memcmp(address, recentClient, sizeof(recentClient)) == 0) return false;
  memcpy(recentClient, address, sizeof(recentClient));
  store_.putBytes(kRecentClientKey, recentClient, sizeof(recentClient));
  return true;
}

void GaugeSettings::restoreDefaultWarnings() {
  tempOffset = kFactoryTempOffset;
  for (uint8_t i = 0; i < kPidCount; i++) warning[i] = pidTable[i].defaultWarning;
}

void GaugeSettings::restoreDefaultEcuOffVolt() {
  ecuOffVolt = kFactoryEcuOffVolt;
}
