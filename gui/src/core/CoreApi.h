// Interface to the mupen64plus core library (libmupen64plus.so.2).
// Implemented by CoreApiImpl (CoreApi.cpp). Factory: CoreApi::create().
#pragma once

#include <string>

#include "m64p_common.h"
#include "m64p_frontend.h"
#include "m64p_types.h"

namespace n64ui {

class ConfigApi;  // fwd

class CoreApi {
 public:
  virtual ~CoreApi() = default;

  // dlopen the core and resolve all m64p_* symbols.
  virtual bool load(const std::string& libPath) = 0;
  virtual void unload() = 0;
  virtual bool isLoaded() const = 0;

  // CoreStartup/CoreShutdown. configDir/dataDir are absolute paths.
  virtual m64p_error startup(const char* configDir, const char* dataDir) = 0;
  virtual void shutdown() = 0;

  // ConfigOverrideUserPaths: make ConfigGetUserDataPath()/CachePath return
  // our dirs (savestates/SRAM/screenshots/mempaks land there).
  virtual m64p_error overrideUserPaths(const char* dataDir,
                                       const char* cacheDir) = 0;

  virtual m64p_error doCommand(m64p_command cmd, int param, void* data) = 0;
  virtual m64p_error attachPlugin(m64p_plugin_type type, void* pluginHandle) = 0;
  virtual m64p_error detachPlugin(m64p_plugin_type type) = 0;
  virtual m64p_error overrideVidExt(const m64p_video_extension_functions* ext) = 0;
  virtual m64p_error addCheat(const char* name, const m64p_cheat_code* codes,
                              int numCodes) = 0;
  virtual m64p_error cheatEnabled(const char* name, int enabled) = 0;

  virtual const char* errorMessage(m64p_error err) const = 0;

  // Raw dlopen handle (passed to PluginStartup as the core handle).
  virtual void* handle() const = 0;

  // Read the core's plugin version struct (PluginGetVersion exported by core).
  virtual m64p_error getPluginVersion(m64p_plugin_type* type, int* version,
                                      int* apiVersion, const char** name,
                                      int* caps) const = 0;

  // State callback: called by the core on M64CORE_* changes (savestate
  // save/load completion, emu state, etc.). Set before startup.
  typedef void (*StateCallback)(int param, int value);
  virtual void setStateCallback(StateCallback cb) = 0;

  static CoreApi* create();
};

}  // namespace n64ui
