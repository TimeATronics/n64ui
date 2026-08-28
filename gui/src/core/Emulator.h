// Emulator facade: high-level control over the core + plugins. This is the
// only object the UI talks to. All commands are safe to call from the main
// thread; M64CMD_EXECUTE runs on a dedicated std::thread.
#pragma once

#include <memory>
#include <string>

#include "m64p_common.h"
#include "m64p_types.h"

namespace n64ui {

class CoreApi;
class ConfigApi;
class PluginManager;
class VidExt;

struct EmulatorConfig {
  std::string libPath;       // dir of libmupen64plus.so.2 ("" = system search)
  std::string pluginDir;     // dir scanned for mupen64plus-*.so plugins
  std::string configDir;     // XDG_CONFIG_HOME
  std::string dataDir;       // XDG_DATA_HOME
  int screenWidth = 1024;
  int screenHeight = 768;
  // Render the game at half resolution (internally) and let the video plugin
  // upscale to the window: big speedup on weak GPUs (llvmpipe, Brick).
  bool halfRes = true;
};

class Emulator {
 public:
  virtual ~Emulator() = default;

  // dlopen core + plugins, CoreStartup, install VidExt, PluginStartup,
  // CoreAttachPlugin for RSP/GFX/AUDIO/INPUT.
  virtual bool init(const EmulatorConfig& cfg) = 0;
  virtual void shutdown() = 0;

  // Load a ROM and start emulation on the core thread. Returns false if the
  // ROM could not be opened. Non-blocking.
  virtual bool launch(const std::string& romPath) = 0;
  virtual void stop() = 0;

  virtual void pause() = 0;
  virtual void resume() = 0;
  virtual void reset(bool hard) = 0;

  // M64EMU_STOPPED/RUNNING/PAUSED.
  virtual int state() const = 0;
  // True once the game has started running (used to detect the game ended
  // vs. it simply not having started yet).
  virtual bool gameStarted() const = 0;

  virtual bool saveState(int slot) = 0;
  virtual bool loadState(int slot) = 0;
  virtual void setStateSlot(int slot) = 0;
  // Explicit-file savestates (M64SAV_M64P). Load auto-detects m64p/pj64.
  virtual bool saveStateToFile(const std::string& path) = 0;
  virtual bool loadStateFromFile(const std::string& path) = 0;

  virtual void setSpeedFactor(int percent) = 0;
  virtual void setSpeedLimiter(bool enabled) = 0;
  virtual void setVolume(int percent) = 0;
  virtual void setMute(bool muted) = 0;
  virtual void takeScreenshot() = 0;
  // Step one frame while paused (M64CMD_ADVANCE_FRAME).
  virtual void frameAdvance() = 0;

  // Live core-state reads (0 when stopped).
  virtual int speedFactor() const = 0;
  virtual bool speedLimiter() const = 0;
  virtual int volume() const = 0;
  virtual bool muted() const = 0;
  virtual int stateSlot() const = 0;

  // Savestate directory (from Core/SaveStatePath), for slot browsing.
  virtual std::string saveDir() const = 0;
  // ROM file MD5 (uppercase) used for the per-game config section.
  virtual std::string romMd5() const = 0;

  // Write the exposed core settings into [<ROM MD5>] (per-game overrides)
  // and save the config file. Returns false when no ROM is open.
  virtual bool saveSettingsPerGame() = 0;

  virtual bool addCheat(const std::string& name, const m64p_cheat_code* codes,
                        int numCodes) = 0;
  virtual void setCheatEnabled(const std::string& name, bool enabled) = 0;

  virtual CoreApi& core() = 0;
  virtual ConfigApi& config() = 0;
  virtual VidExt& vidext() = 0;
  // CLI override for the video plugin name (e.g. "rice"); applied to the
  // PluginManager before the first launch.
  virtual void setGfxPreference(const std::string& name) = 0;

  // Current ROM header name (from M64CMD_ROM_GET_HEADER), empty when idle.
  virtual const std::string& romName() const = 0;

  static Emulator* create();
};

}  // namespace n64ui
