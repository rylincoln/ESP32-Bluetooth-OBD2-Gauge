// Everything the gauge remembers across power-offs. This module alone knows the NVS
// keys, their types and the factory defaults, and devices already in cars hold values
// under these keys, so they have to stay as they are.
#pragma once

#include <stddef.h>
#include <stdint.h>

#include "gauge_tables.h"

// Typed key-value storage. The firmware's version is NVS through Preferences; tests
// use a map. Values of different types live apart, as they do in NVS.
class SettingsStore {
 public:
  virtual void open(const char* ns) = 0;
  virtual uint16_t getUShort(const char* key, uint16_t fallback) = 0;
  virtual void putUShort(const char* key, uint16_t value) = 0;
  virtual bool getBool(const char* key, bool fallback) = 0;
  virtual void putBool(const char* key, bool value) = 0;
  virtual int32_t getInt(const char* key, int32_t fallback) = 0;
  virtual void putInt(const char* key, int32_t value) = 0;
  virtual float getFloat(const char* key, float fallback) = 0;
  virtual void putFloat(const char* key, float value) = 0;
  // These two return false and leave out untouched if nothing that fits is saved.
  virtual bool getString(const char* key, char* out, size_t size) = 0;
  virtual void putString(const char* key, const char* value) = 0;
  virtual bool getBytes(const char* key, uint8_t* out, size_t size) = 0;
  virtual void putBytes(const char* key, const uint8_t* value, size_t size) = 0;

 protected:
  ~SettingsStore() {}
};

class GaugeSettings {
 public:
  explicit GaugeSettings(SettingsStore& store) : store_(store) {}

  // Opens the store and reads everything, so it comes before any other use.
  void load();

  // Screens change these directly. A change is lost at power-off unless saved.
  uint8_t layout = 0;
  bool showSystem = false;
  int8_t tempOffset = 0;
  int warning[kPidCount] = {};  // by PID index
  float ecuOffVolt = 0;

  // The adapter connected to last. saveRecentClient is how this changes.
  uint8_t recentClient[6] = {};

  // saveLayout, saveShowSystem and saveRecentClient skip a value already saved, to
  // spare the flash. saveWarnings and saveEcuOffVolt always write.
  void saveLayout();
  void saveShowSystem();
  void saveWarnings();  // includes tempOffset, which is on the same screen
  void saveEcuOffVolt();
  bool saveRecentClient(const uint8_t* address);  // true if the address was new

  // These change the values above only. The screen saves when it exits.
  void restoreDefaultWarnings();
  void restoreDefaultEcuOffVolt();

 private:
  SettingsStore& store_;
};
