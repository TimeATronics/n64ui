// Curated settings table. One entry per setting the UI exposes; the
// settings screens are generated from this table. Values live in the core's
// mupen64plus.cfg (section/key), read/written via SettingsStore.
#pragma once

#include <cstdint>

namespace n64ui {

enum class SettingKind { Int, Toggle, Choice, Text };

struct Setting {
  const char* section;   // config section in mupen64plus.cfg
  const char* key;
  SettingKind kind;
  const char* label;     // shown in the UI
  const char* def;       // default as string
  int min = 0;           // for Int
  int max = 0;           // for Int
  int step = 1;          // for Int
  const char* choices;   // for Choice: comma-separated labels
};

// All settings the UI can edit. Sections: Core, Video-General,
// Video-Glide64mk2, Audio-SDL, Input-SDL-Control1.
extern const Setting kSettings[];
extern const int kSettingsCount;

}  // namespace n64ui
