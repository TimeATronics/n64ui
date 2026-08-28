// EGL/GLES2 VidExt for the device (PowerVR). Selected at runtime by
// VidExt::create() (see HostVidExt.cpp). Phase 1 fills the real EGL setup.
#include "core/VidExt.h"

#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <functional>

#include "ui/Renderer.h"
#include "util/Log.h"

namespace n64ui {

class VidExtImpl : public VidExt {
 public:
  ~VidExtImpl() override { shutdown(); }

  bool init(int width, int height) override {
    // Phase 1: eglGetDisplay(EGL_DEFAULT_DISPLAY), choose config, create
    // context + surface (validated via the device glprobe), then fill
    // m_functions with the static callbacks below.
    (void)width;
    (void)height;
    LOG_INFO("VidExt: EGL init not implemented yet");
    return false;
  }

  void shutdown() override {}

  void getFunctions(m64p_video_extension_functions* out) override {
    (void)out;
    // Phase 1: fill all 17 callbacks (VidExt_Init, SetVideoMode, GL_GetProc...).
  }

  void setMenuVisible(bool visible) override { m_menu = visible; }
  bool menuVisible() const override { return m_menu; }

  void setRenderer(Renderer* renderer) override { m_renderer = renderer; }

  void setMenuDrawer(MenuDrawer drawer) override { m_drawer = std::move(drawer); }

  void menuChanged() override {}

  void setAutoCallback(std::function<void(int)> cb) override {
    m_autoCb = std::move(cb);
  }

  bool makeCurrent() override {
    // Phase 1 device: eglMakeCurrent.
    return false;
  }

  void releaseCurrent() override {
    // Phase 1 device: eglMakeCurrent(EGL_NO_SURFACE x2, EGL_NO_CONTEXT).
  }

  void swap() override {
    // Phase 1 device: eglSwapBuffers.
  }

  void presentMenu() override {
    // Phase 2: glCopyTexImage2D frozen frame once, draw menu quads, swap.
    LOG_DEBUG("VidExt: menu present not implemented yet");
  }

  void sessionEnd() override {
    releaseCurrent();
  }

  void raiseWindow() override {
    // Device is fullscreen; nothing to raise.
  }

 private:
  bool m_menu = false;
  MenuDrawer m_drawer;
  Renderer* m_renderer = nullptr;
  std::function<void(int)> m_autoCb;
};

VidExt* createEglVidExt() { return new VidExtImpl(); }

}  // namespace n64ui