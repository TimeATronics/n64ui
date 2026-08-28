// SettingsStoreImpl: wraps ConfigApi with string-based get/set for the
// curated table. Skeleton: real read/write lands in Phase 3.
#include "config/SettingsStore.h"

#include "core/ConfigApi.h"
#include "util/Log.h"

namespace n64ui {

class SettingsStoreImpl : public SettingsStore {
 public:
  bool hook(ConfigApi& config) override {
    m_config = &config;
    return true;
  }

  std::vector<const Setting*> settings(const std::string& filter) const override {
    std::vector<const Setting*> out;
    for (int i = 0; i < kSettingsCount; ++i) {
      const Setting& s = kSettings[i];
      if (filter.empty() || filter == s.section) out.push_back(&s);
    }
    return out;
  }

  std::string get(const Setting& s) const override {
    // Phase 3: open section + typed get; fall back to s.def on error.
    (void)s;
    return s.def;
  }

  void set(const Setting& s, const std::string& value) override {
    // Phase 3: typed set + saveSection.
    (void)s;
    (void)value;
    LOG_DEBUG("settings: set %s/%s (Phase 3)", s.section, s.key);
  }

  void save() override {
    if (m_config) m_config->saveFile();
  }

 private:
  ConfigApi* m_config = nullptr;
};

SettingsStore* SettingsStore::create() { return new SettingsStoreImpl(); }

}  // namespace n64ui
