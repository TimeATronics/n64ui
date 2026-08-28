// PlatformImpl: runtime detection + paths.
#include "util/Platform.h"

#include <cstdlib>
#include <cstring>
#include <sys/stat.h>

#include "util/Str.h"

namespace n64ui {

bool Platform::isDevice() {
  struct stat st;
  return stat("/mnt/SDCARD", &st) == 0;
}

bool Platform::isHeadless() {
  const char* drv = getenv("SDL_VIDEODRIVER");
  return drv && strcmp(drv, "dummy") == 0;
}

std::string Platform::configDir() {
  if (isDevice()) {
    const char* xdg = getenv("XDG_CONFIG_HOME");
    return xdg && *xdg ? std::string(xdg) : "/mnt/SDCARD/Emus/N64";
  }
  const char* home = getenv("HOME");
  return home && *home ? strFormat("%s/.config/n64ui", home) : "data/config";
}

std::string Platform::dataDir() {
  if (isDevice()) {
    const char* xdg = getenv("XDG_DATA_HOME");
    return xdg && *xdg ? std::string(xdg) : "/mnt/SDCARD/Emus/N64";
  }
  return "data";
}

std::string Platform::romDir() {
  if (isDevice()) return "/mnt/SDCARD/Roms/N64";
  return "Roms";
}

std::string Platform::emulatorLibPath() {
  // Device: payload sits next to the binary (launch.sh cds there).
  // Host: dlopen default search paths find the system libmupen64plus.
  return isDevice() ? "." : "";
}

std::string Platform::pluginDir() {
  // Device: plugins sit next to the binary. Host: the distro plugin dir.
  if (isDevice()) return ".";
  const char* dirs[] = {"/usr/lib/x86_64-linux-gnu/mupen64plus",
                        "/usr/local/lib/mupen64plus", "/usr/lib/mupen64plus"};
  for (const char* d : dirs) {
    struct stat st;
    if (stat(d, &st) == 0) return std::string(d);
  }
  return "/usr/lib/x86_64-linux-gnu/mupen64plus";
}

int Platform::screenWidth() { return 1024; }
int Platform::screenHeight() { return 768; }

}  // namespace n64ui
