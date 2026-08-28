// CoreApiImpl: dlopen libmupen64plus.so.2, resolve m64p_* symbols, expose them
// through the CoreApi interface.
#include "core/CoreApi.h"

#include <dlfcn.h>

#include "core/Version.h"
#include "util/Log.h"

namespace n64ui {

namespace {

// Function pointer types (from m64p_frontend.h).
typedef m64p_error (*FnCoreStartup)(int, const char*, const char*, void*,
                                    ptr_DebugCallback, void*, ptr_StateCallback, void*);
typedef void (*FnCoreShutdown)(void);
typedef m64p_error (*FnCoreAttachPlugin)(m64p_plugin_type, void*);
typedef m64p_error (*FnCoreDetachPlugin)(m64p_plugin_type);
typedef m64p_error (*FnCoreDoCommand)(m64p_command, int, void*);
typedef m64p_error (*FnCoreOverrideVidExt)(const m64p_video_extension_functions*);
typedef m64p_error (*FnConfigOverrideUserPaths)(const char*, const char*);
typedef m64p_error (*FnCoreAddCheat)(const char*, const m64p_cheat_code*, int);
typedef m64p_error (*FnCoreCheatEnabled)(const char*, int);
typedef const char* (*FnCoreErrorMessage)(m64p_error);
typedef m64p_error (*FnPluginGetVersion)(m64p_plugin_type*, int*, int*, const char**,
                                         int*);

}  // namespace

class CoreApiImpl : public CoreApi {
 public:
  CoreApiImpl() = default;
  ~CoreApiImpl() override { unload(); }

  bool load(const std::string& libPath) override {
    if (m_lib) unload();
    const char* path = libPath.empty() ? "libmupen64plus.so.2" : libPath.c_str();
    m_lib = dlopen(path, RTLD_NOW | RTLD_GLOBAL);
    if (!m_lib) {
      LOG_ERROR("dlopen %s: %s", path, dlerror());
      return false;
    }
    m_startup = (FnCoreStartup)dlsym(m_lib, "CoreStartup");
    m_shutdown = (FnCoreShutdown)dlsym(m_lib, "CoreShutdown");
    m_attach = (FnCoreAttachPlugin)dlsym(m_lib, "CoreAttachPlugin");
    m_detach = (FnCoreDetachPlugin)dlsym(m_lib, "CoreDetachPlugin");
    m_doCommand = (FnCoreDoCommand)dlsym(m_lib, "CoreDoCommand");
    m_overrideVidExt = (FnCoreOverrideVidExt)dlsym(m_lib, "CoreOverrideVidExt");
    m_overridePaths = (FnConfigOverrideUserPaths)dlsym(m_lib, "ConfigOverrideUserPaths");
    m_addCheat = (FnCoreAddCheat)dlsym(m_lib, "CoreAddCheat");
    m_cheatEnabled = (FnCoreCheatEnabled)dlsym(m_lib, "CoreCheatEnabled");
    m_errorMessage = (FnCoreErrorMessage)dlsym(m_lib, "CoreErrorMessage");
    m_getVersion = (FnPluginGetVersion)dlsym(m_lib, "PluginGetVersion");
    if (!m_startup || !m_shutdown || !m_doCommand || !m_getVersion) {
      LOG_ERROR("core symbols incomplete: %s", dlerror());
      unload();
      return false;
    }
    LOG_INFO("core loaded: %s", libPath.c_str());
    return true;
  }

  void unload() override {
    if (m_lib) {
      dlclose(m_lib);
      m_lib = nullptr;
      m_startup = nullptr;
      m_shutdown = nullptr;
      m_attach = nullptr;
      m_detach = nullptr;
      m_doCommand = nullptr;
      m_overrideVidExt = nullptr;
      m_addCheat = nullptr;
      m_cheatEnabled = nullptr;
      m_errorMessage = nullptr;
      m_getVersion = nullptr;
    }
  }

  bool isLoaded() const override { return m_lib != nullptr; }

  m64p_error startup(const char* configDir, const char* dataDir) override {
    return m_startup(kCoreApiVersion, configDir, dataDir, nullptr, DebugCb,
                     nullptr, StateCb, nullptr);
  }

  m64p_error overrideUserPaths(const char* dataDir, const char* cacheDir) override {
    return m_overridePaths ? m_overridePaths(dataDir, cacheDir)
                           : M64ERR_SUCCESS;
  }

  void shutdown() override {
    if (m_shutdown) m_shutdown();
  }

  m64p_error doCommand(m64p_command cmd, int param, void* data) override {
    return m_doCommand(cmd, param, data);
  }

  m64p_error attachPlugin(m64p_plugin_type type, void* pluginHandle) override {
    return m_attach(type, pluginHandle);
  }

  m64p_error detachPlugin(m64p_plugin_type type) override {
    return m_detach(type);
  }

  m64p_error overrideVidExt(const m64p_video_extension_functions* ext) override {
    return m_overrideVidExt(ext);
  }

  m64p_error addCheat(const char* name, const m64p_cheat_code* codes,
                      int numCodes) override {
    return m_addCheat(name, codes, numCodes);
  }

  m64p_error cheatEnabled(const char* name, int enabled) override {
    return m_cheatEnabled(name, enabled);
  }

  const char* errorMessage(m64p_error err) const override {
    return m_errorMessage ? m_errorMessage(err) : "no core";
  }

  void* handle() const override { return m_lib; }

  m64p_error getPluginVersion(m64p_plugin_type* type, int* version, int* apiVersion,
                              const char** name, int* caps) const override {
    return m_getVersion(type, version, apiVersion, name, caps);
  }

  void setStateCallback(StateCallback cb) override { g_stateCb = cb; }

 private:
  static void CALL DebugCb(void* /*ctx*/, int level, const char* message) {
    // The video plugin spams VERBOSE ("UpdateScreen () Origin: ...").
    // Forward everything up to STATUS so useful core messages (e.g.
    // "Saved state to: <file>") are visible; drop the per-frame VERBOSE.
    if (level <= M64MSG_STATUS) LOG_INFO("[core] %s", message);
  }
  static void CALL StateCb(void* /*ctx*/, m64p_core_param param, int value) {
    if (g_stateCb) g_stateCb((int)param, value);
  }

  void* m_lib = nullptr;
  FnCoreStartup m_startup = nullptr;
  FnCoreShutdown m_shutdown = nullptr;
  FnCoreAttachPlugin m_attach = nullptr;
  FnCoreDetachPlugin m_detach = nullptr;
  FnCoreDoCommand m_doCommand = nullptr;
  FnCoreOverrideVidExt m_overrideVidExt = nullptr;
  FnCoreAddCheat m_addCheat = nullptr;
  FnCoreCheatEnabled m_cheatEnabled = nullptr;
  FnCoreErrorMessage m_errorMessage = nullptr;
  FnPluginGetVersion m_getVersion = nullptr;
  FnConfigOverrideUserPaths m_overridePaths = nullptr;
  static StateCallback g_stateCb;
};

CoreApi* CoreApi::create() { return new CoreApiImpl(); }

CoreApiImpl::StateCallback CoreApiImpl::g_stateCb = nullptr;

}  // namespace n64ui
