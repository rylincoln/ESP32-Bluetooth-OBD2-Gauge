// The firmware's SettingsStore: the ESP32's NVS through the Preferences library.
#pragma once

#include <Preferences.h>

#include "gauge_settings.h"

class PreferencesStore : public SettingsStore {
 public:
  explicit PreferencesStore(Preferences& pref) : pref_(pref) {}

  void open(const char* ns) override { pref_.begin(ns, false); }
  uint16_t getUShort(const char* key, uint16_t fallback) override { return pref_.getUShort(key, fallback); }
  void putUShort(const char* key, uint16_t value) override { pref_.putUShort(key, value); }
  bool getBool(const char* key, bool fallback) override { return pref_.getBool(key, fallback); }
  void putBool(const char* key, bool value) override { pref_.putBool(key, value); }
  int32_t getInt(const char* key, int32_t fallback) override { return pref_.getInt(key, fallback); }
  void putInt(const char* key, int32_t value) override { pref_.putInt(key, value); }
  float getFloat(const char* key, float fallback) override { return pref_.getFloat(key, fallback); }
  void putFloat(const char* key, float value) override { pref_.putFloat(key, value); }
  bool getString(const char* key, char* out, size_t size) override { return pref_.getString(key, out, size) > 0; }
  void putString(const char* key, const char* value) override { pref_.putString(key, value); }
  bool getBytes(const char* key, uint8_t* out, size_t size) override { return pref_.getBytes(key, out, size) > 0; }
  void putBytes(const char* key, const uint8_t* value, size_t size) override { pref_.putBytes(key, value, size); }

 private:
  Preferences& pref_;
};
