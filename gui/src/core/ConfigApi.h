// Interface to the core's Config API (m64p_config.h). All settings live in
// mupen64plus.cfg, owned by the core; we only read/write through this API.
#pragma once

#include <string>

#include "m64p_common.h"
#include "m64p_config.h"
#include "m64p_types.h"

namespace n64ui {

class CoreApi;  // fwd

class ConfigApi {
 public:
  virtual ~ConfigApi() = default;

  // Resolve Config* symbols from the loaded core.
  virtual bool hook(CoreApi& core) = 0;

  virtual m64p_error openSection(const char* section, m64p_handle* out) = 0;
  virtual m64p_error saveFile() = 0;
  virtual m64p_error saveSection(const char* section) = 0;
  virtual m64p_error revertChanges() = 0;

  // Typed get/set on an open section handle.
  virtual m64p_error getInt(m64p_handle h, const char* key, int* out) = 0;
  virtual m64p_error getFloat(m64p_handle h, const char* key, float* out) = 0;
  virtual m64p_error getBool(m64p_handle h, const char* key, int* out) = 0;
  virtual m64p_error getString(m64p_handle h, const char* key, char* buf,
                               int* bufSize) = 0;
  virtual m64p_error setInt(m64p_handle h, const char* key, int val) = 0;
  virtual m64p_error setFloat(m64p_handle h, const char* key, float val) = 0;
  virtual m64p_error setBool(m64p_handle h, const char* key, int val) = 0;
  virtual m64p_error setString(m64p_handle h, const char* key,
                               const char* val) = 0;

  static ConfigApi* create();
};

}  // namespace n64ui
