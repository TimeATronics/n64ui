// EmulatorImpl: owns CoreApi/ConfigApi/PluginManager/VidExt and the core
// thread. Init order (mirrors ui-console/RMG): load core -> hook config ->
// CoreStartup -> CoreOverrideVidExt -> load+start plugins -> attach. Launch:
// read ROM -> M64CMD_ROM_OPEN -> EXECUTE on a thread (blocks) -> detach/close.
#include "core/Emulator.h"

#include <cstdio>
#include <cstring>
#include <dlfcn.h>
#include <map>
#include <thread>
#include <vector>

#include <SDL.h>

#include <sys/stat.h>

#include "core/ConfigApi.h"
#include "core/CoreApi.h"
#include "core/PluginManager.h"
#include "core/Version.h"
#include "core/VidExt.h"
#include "util/Log.h"
#include "util/Md5.h"
#include "util/Platform.h"
#include "util/Str.h"

namespace n64ui {

namespace {

// M64CORE_EMU_STATE values (M64EMU_* from m64p_types.h: STOPPED=1,
// RUNNING=2, PAUSED=3).
constexpr int kEmuStopped = 1;
constexpr int kEmuRunning = 2;
constexpr int kEmuPaused = 3;

// Savestate file types for M64CMD_STATE_SAVE (frontend-side convention,
// not in the core headers).
constexpr int kM64SavM64p = 1;
constexpr int kM64SavPj64Zip = 2;
constexpr int kM64SavPj64 = 3;

bool ensureDir(const std::string& dir) {
  if (dir.empty()) return true;
  struct stat st;
  if (stat(dir.c_str(), &st) == 0) return S_ISDIR(st.st_mode);
  return mkdir(dir.c_str(), 0755) == 0;
}

std::vector<unsigned char> readFile(const std::string& path) {
  std::vector<unsigned char> buf;
  FILE* f = fopen(path.c_str(), "rb");
  if (!f) return buf;
  fseek(f, 0, SEEK_END);
  long len = ftell(f);
  fseek(f, 0, SEEK_SET);
  if (len > 0) {
    buf.resize((size_t)len);
    if (fread(buf.data(), 1, (size_t)len, f) != (size_t)len) buf.clear();
  }
  fclose(f);
  return buf;
}

}  // namespace

class EmulatorImpl : public Emulator {
 public:
  EmulatorImpl() {
    m_core.reset(CoreApi::create());
    m_config.reset(ConfigApi::create());
    m_plugins.reset(PluginManager::create());
    m_vidext.reset(VidExt::create());
  }

  ~EmulatorImpl() override { shutdown(); }

  bool init(const EmulatorConfig& cfg) override {
    m_cfg = cfg;
    ensureDir(cfg.configDir);
    ensureDir(cfg.dataDir);

    std::string libPath = cfg.libPath.empty()
                              ? "libmupen64plus.so.2"
                              : cfg.libPath + "/libmupen64plus.so.2";
    if (!m_core->load(libPath)) return false;
    if (!m_config->hook(*m_core)) return false;
    m_core->setStateCallback(&onState);

    if (!m_vidext->init(cfg.screenWidth, cfg.screenHeight)) {
      LOG_ERROR("video init failed");
      return false;
    }
    m_vidext->makeCurrent();

    m64p_error r = m_core->startup(cfg.configDir.c_str(), cfg.dataDir.c_str());
    if (r != M64ERR_SUCCESS) {
      LOG_ERROR("CoreStartup: %s", m_core->errorMessage(r));
      return false;
    }

    // Snapshot the GLOBAL values of every per-game key exactly as loaded
    // from the file (before any per-game overrides apply). "Save for this
    // Game" reverts the global sections to these, so the settings only
    // affect the saved ROM and other games keep their own.
    snapshotGlobalDefaults();

    // Route all core data (savestates, SRAM, mempaks, screenshots) into OUR
    // data dir: ConfigOverrideUserPaths makes ConfigGetUserDataPath() return
    // it (the core's defaults otherwise use the XDG path), and setting the
    // [Core] path keys explicitly makes the savestate file locations match
    // what the frontend scans.
    m_core->overrideUserPaths(cfg.dataDir.c_str(),
                              (cfg.dataDir + "/cache").c_str());
    {
      m64p_handle coreSec = nullptr;
      if (m_config->openSection("Core", &coreSec) == M64ERR_SUCCESS) {
        m_config->setString(coreSec, "SaveStatePath", (cfg.dataDir + "/save").c_str());
        m_config->setString(coreSec, "SaveSRAMPath", (cfg.dataDir + "/save").c_str());
        m_config->setString(coreSec, "ScreenshotPath",
                            (cfg.dataDir + "/screenshot").c_str());
        m_config->saveFile();
      }
    }

    // Free Escape and the joystick stop/fullscreen bindings for our UI:
    // the core's default "Kbd Mapping Stop" is Escape, which would quit the
    // game before our menu can see the key.
    m64p_handle coreEvents = nullptr;
    if (m_config->openSection("CoreEvents", &coreEvents) == M64ERR_SUCCESS) {
      m_config->setInt(coreEvents, "Kbd Mapping Stop", 0);
      m_config->setInt(coreEvents, "Kbd Mapping Fullscreen", 0);
      m_config->setString(coreEvents, "Joy Mapping Stop", "");
      m_config->setString(coreEvents, "Joy Mapping Fullscreen", "");
      m_config->saveFile();
    }

    // The UI is designed for the 1024x768 device screen; force the game to
    // render at that size so the window and the overlay coordinates match on
    // every backend (host windowed and device fullscreen).
    m64p_handle vid = nullptr;
    if (m_config->openSection("Video-General", &vid) == M64ERR_SUCCESS) {
      m_config->setInt(vid, "ScreenWidth", cfg.screenWidth);
      m_config->setInt(vid, "ScreenHeight", cfg.screenHeight);
      m_config->setInt(vid, "ResolutionWidth", cfg.screenWidth);
      m_config->setInt(vid, "ResolutionHeight", cfg.screenHeight);
      m_config->saveFile();
    }

    // Host performance: render the game at half resolution internally and let
    // the video plugin upscale to the window (glide64mk2 wrapper res 3 =
    // 512x384). Device keeps full res until it is proven slow there too.
    if (cfg.halfRes) {
      m64p_handle gl64 = nullptr;
      if (m_config->openSection("Video-Glide64mk2", &gl64) == M64ERR_SUCCESS) {
        m_config->setInt(gl64, "wrpResolution", 3);
        m_config->saveFile();
      }
    }

    // Override VidExt with our implementation: EGL on the device (eager,
    // validated on the Brick), deferred SDL window/context on the host
    // (RMG pattern: attributes collected, created at first SetVideoMode).
    // N64UI_CORE_VIDEXT=1: A/B test - use the core's own vidext (stock).
    if (getenv("N64UI_CORE_VIDEXT")) {
      LOG_INFO("using the core's built-in vidext (A/B test)");
    } else {
      m64p_video_extension_functions ext{};
      m_vidext->getFunctions(&ext);
      if (m_core->overrideVidExt(&ext) != M64ERR_SUCCESS) {
        LOG_ERROR("CoreOverrideVidExt failed");
        return false;
      }
    }

    // Brick input config: force MANUAL mode (0). Mode 2 (fully automatic)
    // makes the plugin copy the "Xbox 360 Controller" auto-config from
    // InputAutoCfg.ini over our bindings at every plugin start, reverting
    // user mappings on the next launch. We manage the bindings ourselves.
    // This must happen BEFORE the plugins start (the plugin reads the config
    // once at PluginStart). First-run defaults are applied only when the
    // controller is still unconfigured (device -1) so user remaps stick.
    if (Platform::isDevice()) {
      m64p_handle inp = nullptr;
      if (m_config->openSection("Input-SDL-Control1", &inp) == M64ERR_SUCCESS) {
        int dev = -2;
        bool fresh = m_config->getInt(inp, "device", &dev) != M64ERR_SUCCESS ||
                     dev == -1;
        bool changed = false;
        int mode = -1;
        if (m_config->getInt(inp, "mode", &mode) != M64ERR_SUCCESS || mode != 0) {
          m_config->setInt(inp, "mode", 0);
          changed = true;
        }
        if (fresh) {
          m_config->setInt(inp, "plugged", 1);
          m_config->setInt(inp, "device", 0);
          m_config->setInt(inp, "mouse", 0);
          m_config->setString(inp, "DPad U", "hat(0 Up)");
          m_config->setString(inp, "DPad D", "hat(0 Down)");
          m_config->setString(inp, "DPad L", "hat(0 Left)");
          m_config->setString(inp, "DPad R", "hat(0 Right)");
          m_config->setString(inp, "A Button", "button(1)");
          m_config->setString(inp, "B Button", "button(0)");
          m_config->setString(inp, "Start", "button(7)");
          m_config->setString(inp, "Z Trig", "axis(2+,24000)");
          m_config->setString(inp, "C Button R", "axis(5+,24000)");
          m_config->setString(inp, "L Trig", "button(4)");
          m_config->setString(inp, "R Trig", "button(5)");
          m_config->setString(inp, "X Axis", "axis(0-,0+)");
          m_config->setString(inp, "Y Axis", "axis(1-,1+)");
          m_config->setString(inp, "AnalogDeadzone", "4096,4096");
          changed = true;
          LOG_INFO("wrote Brick joystick input config (Input-SDL-Control1)");
        }
        if (changed) m_config->saveFile();
      }
    }

    if (!m_plugins->loadAll(cfg.pluginDir)) return false;
    // Plugins are started now but attached after ROM_OPEN (core 2.6.0
    // CoreAttachPlugin requires a ROM to be open first).
    if (m_plugins->startupAll(*m_core, m_core->handle()) != M64ERR_SUCCESS)
      return false;

    LOG_INFO("emulator initialized (core api 0x%06X)", (unsigned)kCoreApiVersion);
    // Debug: what config file does the core use, and what did it load?
    /*
    {
      typedef const char* (*FnGetCfgPath)(void);
      FnGetCfgPath getPath =
          (FnGetCfgPath)dlsym(RTLD_DEFAULT, "ConfigGetUserConfigPath");
      LOG_INFO("config path: %s", getPath ? getPath() : "?");
      m64p_handle sec = nullptr;
      if (m_config->openSection("Input-SDL-Control1", &sec) == M64ERR_SUCCESS) {
        char buf[256] = {0};
        int len = sizeof(buf);
        m64p_error e =
            m_config->getString(sec, "C Button U", buf, &len);
        LOG_INFO("config probe: C Button U (get=%d) = '%s'", (int)e, buf);
      }
    }
    */
    return true;
  }

  void shutdown() override {
    stop();
    if (m_thread && m_thread->joinable()) m_thread->join();
    m_thread.reset();
    m_plugins->shutdownAll();
    m_plugins->unloadAll();
    m_vidext->shutdown();
    m_core->shutdown();
    m_core->unload();
  }

  bool launch(const std::string& romPath) override {
    if (m_thread && m_thread->joinable()) return false;
    m_rom = readFile(romPath);
    if (m_rom.empty()) {
      LOG_ERROR("cannot read ROM %s", romPath.c_str());
      return false;
    }
    m_romMd5 = md5Hex(m_rom);
    applyPerGameSettings();
    m64p_error r = m_core->doCommand(M64CMD_ROM_OPEN, (int)m_rom.size(), m_rom.data());
    if (r != M64ERR_SUCCESS) {
      LOG_ERROR("ROM_OPEN: %s", m_core->errorMessage(r));
      m_rom.clear();
      return false;
    }
    m_rom.clear();  // the core copies the image

    // Core 2.6.0: attach plugins after ROM open, in order GFX, AUDIO, INPUT,
    // RSP (the core warns on any other order).
    for (m64p_plugin_type t : {M64PLUGIN_GFX, M64PLUGIN_AUDIO, M64PLUGIN_INPUT,
                               M64PLUGIN_RSP}) {
      const PluginInfo* p = m_plugins->byType(t);
      if (!p) {
        LOG_ERROR("no %s plugin found", pluginTypeName(t));
        m_core->doCommand(M64CMD_ROM_CLOSE, 0, nullptr);
        return false;
      }
      r = m_core->attachPlugin(t, p->handle);
      if (r != M64ERR_SUCCESS) {
        LOG_ERROR("attach %s: %s", p->name.c_str(), m_core->errorMessage(r));
        m_core->doCommand(M64CMD_ROM_CLOSE, 0, nullptr);
        return false;
      }
      LOG_INFO("attached %s", p->file.c_str());
    }

    m64p_rom_header hdr{};
    r = m_core->doCommand(M64CMD_ROM_GET_HEADER, sizeof(hdr), &hdr);
    if (r == M64ERR_SUCCESS) {
      char name[21] = {0};
      memcpy(name, hdr.Name, 20);
      m_romName = strTrim(std::string(name));
    }
    LOG_INFO("ROM opened: %s", m_romName.c_str());

    m_running = true;
    m_thread.reset(new std::thread([this] { runCore(); }));
    return true;
  }

  void stop() override {
    if (!m_running) return;
    m_running = false;
    LOG_INFO("Emulator::stop() called");
    m_core->doCommand(M64CMD_STOP, 0, nullptr);
  }

  void pause() override {
    if (state() == kEmuRunning) m_core->doCommand(M64CMD_PAUSE, 0, nullptr);
  }

  void resume() override {
    if (state() == kEmuPaused) m_core->doCommand(M64CMD_RESUME, 0, nullptr);
  }

  void reset(bool hard) override {
    if (state() != kEmuStopped)
      m_core->doCommand(M64CMD_RESET, hard ? 1 : 0, nullptr);
  }

  int state() const override {
    return coreState(M64CORE_EMU_STATE);
  }

  bool gameStarted() const override { return g_gameStarted != 0; }

  // The core queues save/load jobs that only run while the CPU executes; when
  // paused, advance one frame at a time until the core reports completion.
  // The few frames run with audio muted so the save doesn't blip sound.
  void flushSavestateJob(int timeoutMs) {
    bool wasMuted = muted();
    if (!wasMuted) setMute(true);
    const int deadline = SDL_GetTicks() + timeoutMs;
    g_stateComplete = 0;
    while (g_stateComplete == 0 && (int)SDL_GetTicks() < deadline) {
      if (m_core->doCommand(M64CMD_ADVANCE_FRAME, 0, nullptr) != M64ERR_SUCCESS)
        break;
      SDL_Delay(10);
    }
    g_stateComplete = 0;
    if (!wasMuted) setMute(false);
  }

  bool saveState(int slot) override {
    if (state() == kEmuStopped) return false;
    setStateSlot(slot);
    if (m_core->doCommand(M64CMD_STATE_SAVE, kM64SavM64p, nullptr) != M64ERR_SUCCESS)
      return false;
    flushSavestateJob(1500);
    return true;
  }

  bool loadState(int slot) override {
    if (state() == kEmuStopped) return false;
    setStateSlot(slot);
    if (m_core->doCommand(M64CMD_STATE_LOAD, 1, nullptr) != M64ERR_SUCCESS)
      return false;
    flushSavestateJob(1500);
    return true;
  }

  bool saveStateToFile(const std::string& path) override {
    if (state() == kEmuStopped) return false;
    if (m_core->doCommand(M64CMD_STATE_SAVE, kM64SavM64p,
                          (void*)path.c_str()) != M64ERR_SUCCESS)
      return false;
    flushSavestateJob(1500);
    return true;
  }

  bool loadStateFromFile(const std::string& path) override {
    if (state() == kEmuStopped) return false;
    if (m_core->doCommand(M64CMD_STATE_LOAD, 1, (void*)path.c_str()) !=
        M64ERR_SUCCESS)
      return false;
    flushSavestateJob(1500);
    return true;
  }

  void setStateSlot(int slot) override {
    m_core->doCommand(M64CMD_CORE_STATE_SET, M64CORE_SAVESTATE_SLOT, &slot);
  }

  void setSpeedFactor(int percent) override {
    if (state() != kEmuStopped)
      m_core->doCommand(M64CMD_CORE_STATE_SET, M64CORE_SPEED_FACTOR, &percent);
  }

  void setSpeedLimiter(bool enabled) override {
    int v = enabled ? 1 : 0;
    if (state() != kEmuStopped)
      m_core->doCommand(M64CMD_CORE_STATE_SET, M64CORE_SPEED_LIMITER, &v);
  }

  void setVolume(int percent) override {
    if (state() != kEmuStopped)
      m_core->doCommand(M64CMD_CORE_STATE_SET, M64CORE_AUDIO_VOLUME, &percent);
  }

  void setMute(bool muted) override {
    int v = muted ? 1 : 0;
    if (state() != kEmuStopped)
      m_core->doCommand(M64CMD_CORE_STATE_SET, M64CORE_AUDIO_MUTE, &v);
  }

  void takeScreenshot() override {
    if (state() != kEmuStopped)
      m_core->doCommand(M64CMD_TAKE_NEXT_SCREENSHOT, 0, nullptr);
  }

  void frameAdvance() override {
    if (state() != kEmuStopped)
      m_core->doCommand(M64CMD_ADVANCE_FRAME, 0, nullptr);
  }

  int speedFactor() const override { return coreState(M64CORE_SPEED_FACTOR); }
  bool speedLimiter() const override { return coreState(M64CORE_SPEED_LIMITER) != 0; }
  int volume() const override { return coreState(M64CORE_AUDIO_VOLUME); }
  bool muted() const override { return coreState(M64CORE_AUDIO_MUTE) != 0; }
  int stateSlot() const override { return coreState(M64CORE_SAVESTATE_SLOT); }

  std::string saveDir() const override {
    std::string dir;
    m64p_handle h = nullptr;
    if (m_config->openSection("Core", &h) == M64ERR_SUCCESS) {
      char buf[1024] = {0};
      int len = sizeof(buf);
      if (m_config->getString(h, "SaveStatePath", buf, &len) == M64ERR_SUCCESS)
        dir = buf;
    }
    if (dir.empty()) dir = Platform::dataDir() + "/save";
    return dir;
  }

  std::string romMd5() const override { return m_romMd5; }

  // The global values of every per-game key, read at startup (before any
  // per-game overrides). Used to keep other games untouched when the user
  // saves settings for one game only.
  std::map<std::string, int> m_globalInt;
  std::map<std::string, std::string> m_globalStr;

  void snapshotGlobalDefaults() {
    for (const PerGameKey* kp = kPerGameKeys; kp->key; ++kp) {
      m64p_handle sec = nullptr;
      int v = 0;
      std::string id = std::string(kp->section) + "|" + kp->key;
      if (m_config->openSection(kp->section, &sec) == M64ERR_SUCCESS &&
          m_config->getInt(sec, kp->key, &v) == M64ERR_SUCCESS)
        m_globalInt[id] = v;
    }
    for (const PerGameKey* kp = kPerGameStringKeys; kp->key; ++kp) {
      m64p_handle sec = nullptr;
      char buf[512] = {0};
      int len = sizeof(buf);
      std::string id = std::string(kp->section) + "|" + kp->key;
      if (m_config->openSection(kp->section, &sec) == M64ERR_SUCCESS &&
          m_config->getString(sec, kp->key, buf, &len) == M64ERR_SUCCESS)
        m_globalStr[id] = buf;
    }
  }

  void restoreGlobalDefaults() {
    for (const PerGameKey* kp = kPerGameKeys; kp->key; ++kp) {
      m64p_handle sec = nullptr;
      std::string id = std::string(kp->section) + "|" + kp->key;
      auto it = m_globalInt.find(id);
      if (it != m_globalInt.end() &&
          m_config->openSection(kp->section, &sec) == M64ERR_SUCCESS)
        m_config->setInt(sec, kp->key, it->second);
    }
    for (const PerGameKey* kp = kPerGameStringKeys; kp->key; ++kp) {
      m64p_handle sec = nullptr;
      std::string id = std::string(kp->section) + "|" + kp->key;
      auto it = m_globalStr.find(id);
      if (it != m_globalStr.end() &&
          m_config->openSection(kp->section, &sec) == M64ERR_SUCCESS)
        m_config->setString(sec, kp->key, it->second.c_str());
    }
  }

  // Every UI-settable setting the per-game save snapshots into [<MD5>] and
  // applyPerGameSettings restores at launch for that ROM only. All other
  // sections stay global.
  struct PerGameKey {
    const char* section;
    const char* key;
  };
  static const PerGameKey kPerGameKeys[];
  static const PerGameKey kPerGameStringKeys[];

  bool saveSettingsPerGame() override {
    if (m_romMd5.empty()) return false;
    m64p_handle game = nullptr;
    if (m_config->openSection(m_romMd5.c_str(), &game) != M64ERR_SUCCESS)
      return false;
    for (const PerGameKey* kp = kPerGameKeys; kp->key; ++kp) { const PerGameKey& k = *kp;
      m64p_handle sec = nullptr;
      int v = 0;
      if (m_config->openSection(k.section, &sec) == M64ERR_SUCCESS &&
          m_config->getInt(sec, k.key, &v) == M64ERR_SUCCESS)
        m_config->setInt(game, k.key, v);
    }
    for (const PerGameKey* kp = kPerGameStringKeys; kp->key; ++kp) { const PerGameKey& k = *kp;
      m64p_handle sec = nullptr;
      char buf[512] = {0};
      int len = sizeof(buf);
      if (m_config->openSection(k.section, &sec) == M64ERR_SUCCESS &&
          m_config->getString(sec, k.key, buf, &len) == M64ERR_SUCCESS)
        m_config->setString(game, k.key, buf);
    }
    // The session edited the GLOBAL sections live; revert them to the
    // startup values so only this ROM gets the settings. In-memory stays as
    // edited (the running game keeps them); the file carries the revert.
    restoreGlobalDefaults();
    m_config->saveFile();
    return true;
  }

  bool addCheat(const std::string& name, const m64p_cheat_code* codes,
                int numCodes) override {
    return m_core->addCheat(name.c_str(), codes, numCodes) == M64ERR_SUCCESS;
  }

  void setCheatEnabled(const std::string& name, bool enabled) override {
    m_core->cheatEnabled(name.c_str(), enabled ? 1 : 0);
  }

  CoreApi& core() override { return *m_core; }
  ConfigApi& config() override { return *m_config; }
  VidExt& vidext() override { return *m_vidext; }

  void setGfxPreference(const std::string& name) override {
    m_plugins->setGfxPreference(name);
  }

  const std::string& romName() const override { return m_romName; }

 private:
  // Core state callback (runs on the emulation thread). The core fires
  // SAVECOMPLETE/LOADCOMPLETE with 1 on success, 0 on failure.
  static void onState(int param, int value) {
    if (param == M64CORE_STATE_SAVECOMPLETE ||
        param == M64CORE_STATE_LOADCOMPLETE)
      g_stateComplete = value > 0 ? 1 : 2;
    if (param == M64CORE_EMU_STATE && value == M64EMU_RUNNING)
      g_gameStarted = 1;
  }

  // Query a live M64CORE_* parameter; returns 0 when the core isn't running.
  int coreState(int param) const {
    int v = 0;
    if (m_core->doCommand(M64CMD_CORE_STATE_QUERY, param, &v) != M64ERR_SUCCESS)
      return 0;
    return v;
  }

  // Apply per-game overrides from [<ROM MD5>] (written by "Save for this
  // Game") onto the [Core] section before the ROM runs.
  void applyPerGameSettings() {
    if (m_romMd5.empty()) return;
    m64p_handle game = nullptr;
    if (m_config->openSection(m_romMd5.c_str(), &game) != M64ERR_SUCCESS) return;
    for (const PerGameKey* kp = kPerGameKeys; kp->key; ++kp) { const PerGameKey& k = *kp;
      // Restore only when a per-game value was saved for this ROM.
      m64p_handle sec = nullptr;
      int v = 0;
      if (m_config->getInt(game, k.key, &v) == M64ERR_SUCCESS &&
          m_config->openSection(k.section, &sec) == M64ERR_SUCCESS)
        m_config->setInt(sec, k.key, v);
    }
    for (const PerGameKey* kp = kPerGameStringKeys; kp->key; ++kp) { const PerGameKey& k = *kp;
      m64p_handle sec = nullptr;
      char buf[512] = {0};
      int len = sizeof(buf);
      if (m_config->getString(game, k.key, buf, &len) == M64ERR_SUCCESS &&
          m_config->openSection(k.section, &sec) == M64ERR_SUCCESS)
        m_config->setString(sec, k.key, buf);
    }
    LOG_INFO("applied per-game settings (%s)", m_romMd5.c_str());
  }

  static const char* pluginTypeName(m64p_plugin_type t) {
    switch (t) {
      case M64PLUGIN_RSP: return "RSP";
      case M64PLUGIN_GFX: return "GFX";
      case M64PLUGIN_AUDIO: return "AUDIO";
      case M64PLUGIN_INPUT: return "INPUT";
      default: return "?";
    }
  }

  void runCore() {
    // The video plugin needs the GL context current on this thread. The UI
    // thread releases it after its draws; retry until we can grab it.
    for (int i = 0; i < 100 && !m_vidext->makeCurrent(); ++i) {
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    LOG_INFO("core thread: executing");
    m64p_error r = m_core->doCommand(M64CMD_EXECUTE, 0, nullptr);
    LOG_INFO("core thread: execute returned (%s)", m_core->errorMessage(r));
    for (m64p_plugin_type t : {M64PLUGIN_GFX, M64PLUGIN_AUDIO, M64PLUGIN_INPUT,
                               M64PLUGIN_RSP}) {
      m_core->detachPlugin(t);
    }
    m_core->doCommand(M64CMD_ROM_CLOSE, 0, nullptr);
    m_romName.clear();
    m_romMd5.clear();
    m_vidext->sessionEnd();
  }

  EmulatorConfig m_cfg;
  std::unique_ptr<CoreApi> m_core;
  std::unique_ptr<ConfigApi> m_config;
  std::unique_ptr<PluginManager> m_plugins;
  std::unique_ptr<VidExt> m_vidext;
  std::unique_ptr<std::thread> m_thread;
  std::vector<unsigned char> m_rom;
  std::string m_romName;
  std::string m_romMd5;
  bool m_running = false;
  static volatile int g_stateComplete;
  static volatile int g_gameStarted;
};

Emulator* Emulator::create() { return new EmulatorImpl(); }
volatile int EmulatorImpl::g_stateComplete = 0;
volatile int EmulatorImpl::g_gameStarted = 0;

// Numeric per-game settings (core emulation + video/audio options).
const EmulatorImpl::PerGameKey EmulatorImpl::kPerGameKeys[] = {
    {"Core", "R4300Emulator"},
    {"Core", "DisableExtraMem"},
    {"Core", "CountPerOp"},
    {"Core", "SiDmaDuration"},
    {"Core", "RandomizeInterrupt"},
    {"Video-General", "VerticalSync"},
    {"Video-Glide64mk2", "wrpResolution"},
    {"Audio-SDL", "DEFAULT_FREQUENCY"},
    {"Audio-SDL", "SWAP_CHANNELS"},
    {nullptr, nullptr},
};

// String per-game settings (input bindings; the input-sdl plugin's live
// bindings are read at plugin start, so these apply from the next launch).
const EmulatorImpl::PerGameKey EmulatorImpl::kPerGameStringKeys[] = {
    {"Audio-SDL", "RESAMPLE"},
    {"Input-SDL-Control1", "A Button"},
    {"Input-SDL-Control1", "B Button"},
    {"Input-SDL-Control1", "Start"},
    {"Input-SDL-Control1", "Z Trig"},
    {"Input-SDL-Control1", "DPad U"},
    {"Input-SDL-Control1", "DPad D"},
    {"Input-SDL-Control1", "DPad L"},
    {"Input-SDL-Control1", "DPad R"},
    {"Input-SDL-Control1", "C Button U"},
    {"Input-SDL-Control1", "C Button D"},
    {"Input-SDL-Control1", "C Button L"},
    {"Input-SDL-Control1", "C Button R"},
    {"Input-SDL-Control1", "L Trig"},
    {"Input-SDL-Control1", "R Trig"},
    {"Input-SDL-Control1", "X Axis"},
    {"Input-SDL-Control1", "Y Axis"},
    {"Input-SDL-Control1", "Mempak switch"},
    {"Input-SDL-Control1", "Rumblepak switch"},
    {nullptr, nullptr},
};

}  // namespace n64ui
