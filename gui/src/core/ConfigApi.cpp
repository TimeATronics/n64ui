// ConfigApiImpl: thin typed wrapper over the core's Config* exports.
#include "core/ConfigApi.h"

#include <dlfcn.h>

#include "core/CoreApi.h"
#include "util/Log.h"

namespace n64ui {

namespace {
typedef m64p_error (*FnConfigOpenSection)(const char*, m64p_handle*);
typedef m64p_error (*FnConfigSaveFile)(void);
typedef m64p_error (*FnConfigSaveSection)(const char*);
typedef m64p_error (*FnConfigRevertChanges)(void);
typedef m64p_error (*FnConfigSetParameter)(m64p_handle, const char*, m64p_type,
                                           const void*);
// Note: ConfigGetParameter takes ParamType and MaxSize BY VALUE (see
// m64p_config.h); getting the type back uses ConfigGetParameterType.
typedef m64p_error (*FnConfigGetParameter)(m64p_handle, const char*, m64p_type,
                                           void*, int);
typedef m64p_error (*FnConfigGetParameterType)(m64p_handle, const char*,
                                               m64p_type*);
}  // namespace

class ConfigApiImpl : public ConfigApi {
 public:
  bool hook(CoreApi& core) override {
    // Reuse the core's dlopen handle via dlsym on RTLD_DEFAULT: the core was
    // loaded RTLD_GLOBAL by CoreApi, so its exports are visible here.
    (void)core;
    m_open = (FnConfigOpenSection)dlsym(RTLD_DEFAULT, "ConfigOpenSection");
    m_save = (FnConfigSaveFile)dlsym(RTLD_DEFAULT, "ConfigSaveFile");
    m_saveSection = (FnConfigSaveSection)dlsym(RTLD_DEFAULT, "ConfigSaveSection");
    m_revert = (FnConfigRevertChanges)dlsym(RTLD_DEFAULT, "ConfigRevertChanges");
    m_set = (FnConfigSetParameter)dlsym(RTLD_DEFAULT, "ConfigSetParameter");
    m_get = (FnConfigGetParameter)dlsym(RTLD_DEFAULT, "ConfigGetParameter");
    if (!m_open || !m_save || !m_get || !m_set) {
      LOG_ERROR("ConfigApi: missing Config* symbols");
      return false;
    }
    return true;
  }

  m64p_error openSection(const char* section, m64p_handle* out) override {
    return m_open(section, out);
  }
  m64p_error saveFile() override { return m_save(); }
  m64p_error saveSection(const char* section) override { return m_saveSection(section); }
  m64p_error revertChanges() override { return m_revert(); }

  m64p_error getInt(m64p_handle h, const char* key, int* out) override {
    return m_get(h, key, M64TYPE_INT, out, (int)sizeof(*out));
  }
  m64p_error getFloat(m64p_handle h, const char* key, float* out) override {
    return m_get(h, key, M64TYPE_FLOAT, out, (int)sizeof(*out));
  }
  m64p_error getBool(m64p_handle h, const char* key, int* out) override {
    return m_get(h, key, M64TYPE_BOOL, out, (int)sizeof(*out));
  }
  m64p_error getString(m64p_handle h, const char* key, char* buf, int* bufSize) override {
    return m_get(h, key, M64TYPE_STRING, buf, *bufSize);
  }
  m64p_error setInt(m64p_handle h, const char* key, int val) override {
    return set(h, key, M64TYPE_INT, &val);
  }
  m64p_error setFloat(m64p_handle h, const char* key, float val) override {
    return set(h, key, M64TYPE_FLOAT, &val);
  }
  m64p_error setBool(m64p_handle h, const char* key, int val) override {
    return set(h, key, M64TYPE_BOOL, &val);
  }
  m64p_error setString(m64p_handle h, const char* key, const char* val) override {
    return set(h, key, M64TYPE_STRING, val);
  }

 private:
  m64p_error get(m64p_handle h, const char* key, m64p_type type, void* buf,
                 int size) {
    return m_get(h, key, type, buf, size);
  }
  m64p_error set(m64p_handle h, const char* key, m64p_type type, const void* val) {
    return m_set(h, key, type, val);
  }

  FnConfigOpenSection m_open = nullptr;
  FnConfigSaveFile m_save = nullptr;
  FnConfigSaveSection m_saveSection = nullptr;
  FnConfigRevertChanges m_revert = nullptr;
  FnConfigSetParameter m_set = nullptr;
  FnConfigGetParameter m_get = nullptr;
};

ConfigApi* ConfigApi::create() { return new ConfigApiImpl(); }

}  // namespace n64ui
