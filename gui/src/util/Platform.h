// Platform detection + path resolution. Same pattern as music_player:
// runtime detection only, no build tags. The device always has /mnt/SDCARD.
#pragma once

#include <string>

namespace n64ui {

class Platform {
 public:
  // True on the TrimUI Brick (stock firmware).
  static bool isDevice();

  // True when SDL_VIDEODRIVER=dummy (CI / headless tests).
  static bool isHeadless();

  // Config dir (XDG_CONFIG_HOME on device, ~/.config/n64ui on host).
  static std::string configDir();

  // Data dir (XDG_DATA_HOME on device, ./data on host).
  static std::string dataDir();

  // ROM browser root (device: <N64 dir>/../../Roms/N64, host: ./Roms).
  static std::string romDir();

  // Where the emulator payload (libmupen64plus + plugins) lives.
  // Device: the dir of the running binary. Host: system search path.
  static std::string emulatorLibPath();

  // Directory scanned for mupen64plus-*.so plugins.
  static std::string pluginDir();

  static int screenWidth();
  static int screenHeight();
};

}  // namespace n64ui
