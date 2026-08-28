// SettingsStore: reads/writes the curated settings table through the core
// Config API. All changes are applied live; save() persists mupen64plus.cfg.
#pragma once

#include <string>
#include <vector>

#include "config/Settings.h"

namespace n64ui {

class ConfigApi;

class SettingsStore {
 public:
  virtual ~SettingsStore() = default;

  virtual bool hook(ConfigApi& config) = 0;

  // All settings (optionally filtered by section).
  virtual std::vector<const Setting*> settings(const std::string& sectionFilter) const = 0;

  virtual std::string get(const Setting& s) const = 0;
  virtual void set(const Setting& s, const std::string& value) = 0;

  // ConfigSaveFile.
  virtual void save() = 0;

  static SettingsStore* create();
};

}  // namespace n64ui
