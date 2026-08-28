// PluginManagerImpl: scan dir for mupen64plus-*.so, dlopen, classify by type.
#include "core/PluginManager.h"

#include <dlfcn.h>
#include <dirent.h>

#include "core/CoreApi.h"
#include "util/Log.h"
#include "util/Platform.h"
#include "util/Str.h"

namespace n64ui {

namespace {
typedef m64p_error (*FnPluginStartup)(void*, void*, void (*)(void*, int, const char*));
typedef void (*FnPluginShutdown)(void);
typedef m64p_error (*FnPluginGetVersion)(m64p_plugin_type*, int*, int*, const char**,
                                         int*);

// Plugin debug messages go through this (the CLI passes its own callback).
// glide64mk2 spams M64MSG_VERBOSE per-frame ("UpdateScreen () ..."); forward
// everything up to STATUS (useful startup/status info, e.g. Rice's GL init)
// and drop only the per-frame VERBOSE chatter.
void pluginDebug(void* /*ctx*/, int level, const char* message) {
  if (level <= M64MSG_STATUS) LOG_INFO("[plugin] %s", message);
}
}  // namespace

class PluginManagerImpl : public PluginManager {
 public:
  bool loadAll(const std::string& dir) override {
    unloadAll();
    if (dir.empty()) {
      LOG_INFO("plugins: no plugin dir configured (host skeleton mode)");
      return true;
    }
    DIR* d = opendir(dir.c_str());
    if (!d) {
      LOG_ERROR("cannot open plugin dir %s", dir.c_str());
      return false;
    }
    dirent* e;
    while ((e = readdir(d)) != nullptr) {
      std::string name = e->d_name;
      if (!strStartsWith(name, "mupen64plus-") || !strEndsWith(name, ".so")) continue;
      if (name.find("-video-") == std::string::npos &&
          name.find("-audio-") == std::string::npos &&
          name.find("-input-") == std::string::npos &&
          name.find("-rsp-") == std::string::npos)
        continue;  // not one of our plugin types
      loadOne(dir + "/" + name);
    }
    closedir(d);
    LOG_INFO("plugins loaded: %zu", m_plugins.size());
    return true;
  }

  void unloadAll() override {
    for (auto& p : m_plugins)
      if (p.handle) dlclose(p.handle);
    m_plugins.clear();
  }

  const std::vector<PluginInfo>& all() const override { return m_plugins; }

  void setGfxPreference(const std::string& name) override {
    m_gfxPrefer = name;
  }

  const PluginInfo* byType(m64p_plugin_type type) const override {
    // Prefer the canonical plugin by name (CLI override for GFX first).
    const char* prefer = nullptr;
    switch (type) {
      case M64PLUGIN_RSP: prefer = "rsp-hle"; break;
      case M64PLUGIN_GFX:
        // Default everywhere: glide64mk2 (adapts to the UI's minimal GL
        // context); a CLI --video <name> override picks another plugin.
        prefer = m_gfxPrefer.empty() ? "glide64mk2" : m_gfxPrefer.c_str();
        break;
      case M64PLUGIN_AUDIO: prefer = "audio-sdl"; break;
      case M64PLUGIN_INPUT: prefer = "input-sdl"; break;
      default: break;
    }
    const PluginInfo* fallback = nullptr;
    for (const auto& p : m_plugins) {
      if (p.type != type) continue;
      if (!fallback) fallback = &p;
      if (prefer && p.file.find(prefer) != std::string::npos) return &p;
    }
    return fallback;
  }

  m64p_error startupAll(CoreApi& core, void* coreHandle) override {
    (void)core;
    for (auto& p : m_plugins) {
      m64p_error r = startupOne(core, coreHandle, p);
      if (r != M64ERR_SUCCESS) return r;
    }
    return M64ERR_SUCCESS;
  }

  m64p_error startupOne(CoreApi& core, void* coreHandle,
                        const PluginInfo& p) override {
    FnPluginStartup fn = (FnPluginStartup)p.startupFn;
    m64p_error r = fn(coreHandle, (void*)p.name.c_str(), pluginDebug);
    if (r != M64ERR_SUCCESS) {
      LOG_ERROR("PluginStartup %s: %s", p.name.c_str(), core.errorMessage(r));
      return r;
    }
    return M64ERR_SUCCESS;
  }

  void shutdownAll() override {
    for (auto& p : m_plugins) {
      FnPluginShutdown fn = (FnPluginShutdown)p.shutdownFn;
      if (fn) fn();
    }
  }

 private:
  void loadOne(const std::string& path) {
    void* h = dlopen(path.c_str(), RTLD_NOW | RTLD_GLOBAL);
    if (!h) {
      LOG_WARN("dlopen %s: %s", path.c_str(), dlerror());
      return;
    }
    auto getVersion = (FnPluginGetVersion)dlsym(h, "PluginGetVersion");
    if (!getVersion) {
      dlclose(h);
      return;
    }
    PluginInfo pi;
    pi.handle = h;
    const char* name = nullptr;
    m64p_error r = getVersion(&pi.type, &pi.version, nullptr, &name, nullptr);
    if (r != M64ERR_SUCCESS || pi.type == M64PLUGIN_NULL ||
        pi.type == M64PLUGIN_CORE) {
      dlclose(h);
      return;
    }
    pi.name = name ? name : strBaseName(path);
    pi.file = strBaseName(path);
    pi.startupFn = dlsym(h, "PluginStartup");
    pi.shutdownFn = dlsym(h, "PluginShutdown");
    m_plugins.push_back(pi);
    LOG_INFO("plugin: [%d] %s v%d", (int)pi.type, pi.name.c_str(), pi.version);
  }

  std::vector<PluginInfo> m_plugins;
  std::string m_gfxPrefer;
};

PluginManager* PluginManager::create() { return new PluginManagerImpl(); }

}  // namespace n64ui
