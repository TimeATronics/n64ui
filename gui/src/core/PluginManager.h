// Plugin discovery/loading: dlopen mupen64plus-*.so from the emulator dir,
// classify by PluginGetVersion, start them and attach to the core.
#pragma once

#include <string>
#include <vector>

#include "m64p_common.h"
#include "m64p_plugin.h"
#include "m64p_types.h"

namespace n64ui {

class CoreApi;  // fwd

struct PluginInfo {
  m64p_plugin_type type = M64PLUGIN_NULL;
  void* handle = nullptr;  // dlopen handle
  void* startupFn = nullptr;
  void* shutdownFn = nullptr;
  std::string name;    // from PluginGetVersion (display)
  std::string file;    // basename of the .so (used for selection)
  int version = 0;
};

class PluginManager {
 public:
  virtual ~PluginManager() = default;

  // dlopen every mupen64plus-*.so in dir (except the core lib) and classify.
  virtual bool loadAll(const std::string& dir) = 0;
  virtual void unloadAll() = 0;

  virtual const std::vector<PluginInfo>& all() const = 0;
  virtual const PluginInfo* byType(m64p_plugin_type type) const = 0;
  // Optional CLI override for the GFX plugin (e.g. "rice"): the next
  // byType(M64PLUGIN_GFX) call prefers the plugin whose name contains it.
  virtual void setGfxPreference(const std::string& name) = 0;

  // PluginStartup/PluginShutdown for every loaded plugin.
  virtual m64p_error startupAll(CoreApi& core, void* coreHandle) = 0;
  virtual void shutdownAll() = 0;

  // PluginStartup for a single plugin (used before attaching it).
  virtual m64p_error startupOne(CoreApi& core, void* coreHandle,
                                const PluginInfo& p) = 0;

  static PluginManager* create();
};

}  // namespace n64ui
