// GameMenuImpl: the in-game pause menu, built on the plug-and-play MenuScreen
// base. Add new entries by calling add() in the constructor.
#include "screens/GameMenu.h"

#include <cstdio>
#include <ctime>
#include <chrono>
#include <functional>
#include <thread>

#include <dirent.h>
#include <sys/stat.h>

#include "core/ConfigApi.h"
#include "core/CoreApi.h"
#include "screens/InputMapping.h"
#include "screens/MenuScreen.h"
#include "util/Log.h"
#include "util/Str.h"

namespace n64ui {

namespace {

ScreenResult noop() { return {ScreenResult::None, nullptr}; }

// A list of speed factors the UI offers (25..300 in 25% steps).
constexpr int kSpeedSteps[] = {25, 50, 75, 100, 125, 150, 175,
                               200, 225, 250, 275, 300};

bool readConfigBool(Emulator& emu, const char* section, const char* key) {
  int v = 0;
  m64p_handle h = nullptr;
  if (emu.config().openSection(section, &h) == M64ERR_SUCCESS)
    emu.config().getBool(h, key, &v);
  return v != 0;
}

void writeConfigBool(Emulator& emu, const char* section, const char* key,
                     bool on) {
  m64p_handle h = nullptr;
  if (emu.config().openSection(section, &h) == M64ERR_SUCCESS) {
    emu.config().setBool(h, key, on ? 1 : 0);
    emu.config().saveFile();
  }
}

int readConfigInt(Emulator& emu, const char* section, const char* key) {
  int v = 0;
  m64p_handle h = nullptr;
  if (emu.config().openSection(section, &h) == M64ERR_SUCCESS)
    emu.config().getInt(h, key, &v);
  return v;
}

void writeConfigInt(Emulator& emu, const char* section, const char* key,
                    int v) {
  m64p_handle h = nullptr;
  if (emu.config().openSection(section, &h) == M64ERR_SUCCESS) {
    emu.config().setInt(h, key, v);
    emu.config().saveFile();
  }
}

void writeConfigString(Emulator& emu, const char* section, const char* key,
                       const char* v) {
  m64p_handle h = nullptr;
  if (emu.config().openSection(section, &h) == M64ERR_SUCCESS) {
    emu.config().setString(h, key, v);
    emu.config().saveFile();
  }
}

}  // namespace

GameMenu::GameMenu(Emulator& e, Input& input) : MenuScreen(e), m_input(&input) {
  add("Resume", [] { return screenPop(); });
  add("Save State", [this] { return screenPush(new SlotMenu(emu(), true)); });
  add("Load State", [this] { return screenPush(new SlotMenu(emu(), false)); });
  add("Reset", [this] { return screenPush(new ResetMenu(emu())); });
  add("Speed", [this] { return screenPush(new SpeedMenu(emu())); },
      [this] { return strFormat("%d%%", emu().speedFactor()); });
  add("Video", [this] { return screenPush(new VideoMenu(emu())); });
  add("Audio", [this] { return screenPush(new AudioMenu(emu())); });
  add("Input", [this] { return screenPush(new InputMappingMenu(emu())); });
  add("Save Settings", [this] { return screenPush(new SaveSettingsMenu(emu())); });
  add("Exit", [this] {
    emu().stop();
    return screenPop();
  });
}

GameMenu::~GameMenu() = default;

void GameMenu::onShow() {
  // The full GL-state save/restore now keeps the video plugin's rendering
  // intact across the pause, so pause works on both host and device.
  emu().pause();
  LOG_INFO("game menu open (paused)");
}

void GameMenu::onHide() {
  // Let the user release the closing key (Enter/B/Menu) before the core
  // resumes, so it never reaches the emulated controller.
  if (m_input) m_input->waitForRelease(400);
  emu().resume();
  LOG_INFO("game menu closed (resumed)");
}

std::string GameMenu::title() const {
  // Show the running game's name (from the ROM header) as the menu header.
  const std::string& name = emu().romName();
  return name.empty() ? "N64" : name;
}
std::string GameMenu::footer() const { return "A: select  B: resume"; }

// --- Slot picker (save/load states 0..9 with per-slot file mtimes) ---

SlotMenu::SlotMenu(Emulator& e, bool save) : MenuScreen(e), m_save(save) {
  for (int i = 0; i < 10; ++i) {
    int slot = i;
    add(slotName(i), [this, slot] {
      bool ok = m_save ? emu().saveState(slot) : emu().loadState(slot);
      showToast(ok ? (m_save ? "State saved" : "State loaded")
                   : (m_save ? "Save failed" : "Load failed"));
      LOG_INFO("menu: %s state slot %d (%s)", m_save ? "saved" : "loaded",
               slot, ok ? "ok" : "failed");
      return noop();
    }, [this, slot] { return slotInfo(slot); });
  }
}
std::string SlotMenu::title() const {
  return m_save ? "Save State" : "Load State";
}
std::string SlotMenu::slotName(int i) const {
  return strFormat("Slot %d", i);
}
std::string SlotMenu::slotInfo(int slot) const {
  // The core names files <goodname>-<md5>.st<N>; scan the save dir for any
  // *.st<N> (whatever the goodname) and show the newest mtime.
  const std::string suffix = strFormat(".st%d", slot);
  const std::string dir = emu().saveDir();
  struct dirent* e;
  DIR* d = opendir(dir.c_str());
  if (!d) return "(empty)";
  time_t best = 0;
  while ((e = readdir(d)) != nullptr) {
    if (strstr(e->d_name, suffix.c_str()) == nullptr) continue;
    size_t len = strlen(e->d_name);
    if (len < suffix.size() ||
        strcmp(e->d_name + len - suffix.size(), suffix.c_str()) != 0)
      continue;
    struct stat st;
    std::string path = dir + "/" + e->d_name;
    if (stat(path.c_str(), &st) == 0 && st.st_mtime > best)
      best = st.st_mtime;
  }
  closedir(d);
  if (best == 0) return "(empty)";
  char buf[32];
  struct tm tmv;
  localtime_r(&best, &tmv);
  strftime(buf, sizeof(buf), "%d/%m %H:%M", &tmv);
  return buf;
}

// --- Reset submenu ---

ResetMenu::ResetMenu(Emulator& e) : MenuScreen(e) {
  add("Soft Reset", [this] {
    emu().reset(false);
    LOG_INFO("menu: soft reset");
    return screenPop();
  });
  add("Hard Reset", [this] {
    emu().reset(true);
    LOG_INFO("menu: hard reset");
    return screenPop();
  });
}
std::string ResetMenu::title() const { return "Reset"; }

// --- Speed submenu ---

SpeedMenu::SpeedMenu(Emulator& e) : MenuScreen(e) {
  for (int f : kSpeedSteps) {
    add(strFormat("%d%%", f), [this, f] {
      emu().setSpeedFactor(f);
      showToast(strFormat("Speed set to %d%%", f));
      return screenPop();
    }, [this, f] {
      return emu().speedFactor() == f ? "current" : "";
    });
  }
  add("Speed Limiter", [this] {
    emu().setSpeedLimiter(!emu().speedLimiter());
    return noop();
  }, [this] { return emu().speedLimiter() ? "on" : "off"; });
}
std::string SpeedMenu::title() const { return "Speed"; }

// --- Video submenu (no volume/brightness; fullscreen is automatic on the
// Brick so it is not exposed here) ---

VideoMenu::VideoMenu(Emulator& e) : MenuScreen(e) {
  add("Vertical Sync", [this] {
    writeConfigBool(emu(), "Video-General", "VerticalSync",
                    !readConfigBool(emu(), "Video-General", "VerticalSync"));
    return noop();
  }, [this] { return readConfigBool(emu(), "Video-General", "VerticalSync") ? "on" : "off"; });
  add("Wrapper Resolution", [this] { return screenPush(new WrapperResMenu(emu())); },
      [this] { return wrapperResName(); });
  addSaveEntries();
}
std::string VideoMenu::title() const { return "Video"; }

std::string VideoMenu::wrapperResName() const {
  switch (readConfigInt(emu(), "Video-Glide64mk2", "wrpResolution")) {
    case 3: return "512x384";
    case 4: return "640x480";
    case 5: return "800x600";
    case 6: return "1024x768";
    case 7: return "1280x1024";
    default: return "auto";
  }
}

WrapperResMenu::WrapperResMenu(Emulator& e) : MenuScreen(e) {
  add("320x240", [this] { setRes(2); return screenPop(); });
  add("512x384 (half)", [this] { setRes(3); return screenPop(); });
  add("640x480", [this] { setRes(4); return screenPop(); });
  add("800x600", [this] { setRes(5); return screenPop(); });
  add("1024x768 (full)", [this] { setRes(6); return screenPop(); });
  addSaveEntries();
}
std::string WrapperResMenu::title() const { return "Wrapper Resolution"; }
void WrapperResMenu::setRes(int v) {
  writeConfigInt(emu(), "Video-Glide64mk2", "wrpResolution", v);
  showToast("Applies when the game restarts");
}

// --- Audio submenu (mute + channel/format options; NO volume/brightness) ---

AudioMenu::AudioMenu(Emulator& e) : MenuScreen(e) {
  add("Mute", [this] {
    emu().setMute(!emu().muted());
    return noop();
  }, [this] { return emu().muted() ? "on" : "off"; });
  add("Swap Channels", [this] {
    writeConfigBool(emu(), "Audio-SDL", "SWAP_CHANNELS",
                    !readConfigBool(emu(), "Audio-SDL", "SWAP_CHANNELS"));
    return noop();
  }, [this] { return readConfigBool(emu(), "Audio-SDL", "SWAP_CHANNELS") ? "on" : "off"; });
  add("Sample Rate", [this] { return screenPush(new SampleRateMenu(emu())); },
      [this] { return strFormat("%d Hz",
          readConfigInt(emu(), "Audio-SDL", "DEFAULT_FREQUENCY")); });
  add("Resampler", [this] { return screenPush(new ResamplerMenu(emu())); },
      [this] { return resamplerName(); });
  addSaveEntries();
}
std::string AudioMenu::title() const { return "Audio"; }

std::string AudioMenu::resamplerName() const {
  char buf[256] = {0};
  int len = sizeof(buf);
  m64p_handle h = nullptr;
  if (emu().config().openSection("Audio-SDL", &h) == M64ERR_SUCCESS &&
      emu().config().getString(h, "RESAMPLE", buf, &len) == M64ERR_SUCCESS)
    return buf;
  return "src-linear";
}

SampleRateMenu::SampleRateMenu(Emulator& e) : MenuScreen(e) {
  for (int rate : {32000, 33600, 44100, 48000}) {
    add(strFormat("%d Hz", rate), [this, rate] {
      writeConfigInt(emu(), "Audio-SDL", "DEFAULT_FREQUENCY", rate);
      showToast("Applies when the game restarts");
      return screenPop();
    }, [this, rate] {
      return readConfigInt(emu(), "Audio-SDL", "DEFAULT_FREQUENCY") == rate
                 ? "current"
                 : "";
    });
  }
  addSaveEntries();
}
std::string SampleRateMenu::title() const { return "Sample Rate"; }

ResamplerMenu::ResamplerMenu(Emulator& e) : MenuScreen(e) {
  // Real audio-sdl RESAMPLE values (from the plugin's help text).
  for (const char* name : {"trivial", "speex-fixed-10", "src-linear",
                           "src-sinc-fastest", "src-sinc-medium-quality",
                           "src-sinc-best-quality"}) {
    add(name, [this, name] {
      writeConfigString(emu(), "Audio-SDL", "RESAMPLE", name);
      showToast("Applies when the game restarts");
      return screenPop();
    }, [this, name] {
      char buf[256] = {0};
      int len = sizeof(buf);
      m64p_handle h = nullptr;
      std::string cur;
      if (emu().config().openSection("Audio-SDL", &h) == M64ERR_SUCCESS &&
          emu().config().getString(h, "RESAMPLE", buf, &len) == M64ERR_SUCCESS)
        cur = buf;
      return cur == name ? "current" : "";
    });
  }
  addSaveEntries();
}
std::string ResamplerMenu::title() const { return "Resampler"; }

// --- Save settings: global vs per-game ---

SaveSettingsMenu::SaveSettingsMenu(Emulator& e) : MenuScreen(e) {
  add("Global", [this] {
    emu().config().saveFile();
    showToast("Settings saved globally");
    LOG_INFO("menu: settings saved globally");
    return screenPop();
  });
  add("This Game", [this] {
    if (emu().saveSettingsPerGame()) {
      showToast("Settings saved for this game");
      LOG_INFO("menu: settings saved for this game");
    }
    return screenPop();
  });
}
std::string SaveSettingsMenu::title() const { return "Save Settings"; }

// --- Cheats (Phase 5: mupencheat.txt parsing + CheatScreen) ---

}  // namespace n64ui
