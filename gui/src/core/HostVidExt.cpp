// Host VidExt: ONE window shared by the game and the UI.
//
// Threading model (single GL context, single current-owner at a time):
//   - init(): creates the window + context EAGERLY on the main thread with
//     minimal attributes, so the launcher can be drawn before any game runs.
//   - setMode() (runs on the core's emulation thread): hands the context over
//     to that thread (it was released by the main thread after the launcher).
//     From then on the context stays current on the emulation thread and the
//     main thread never touches GL again.
//   - swap() runs on the emulation thread and is called by the core even while
//     the game is paused (mupen64plus pause_loop -> VidExt_GL_SwapBuffers).
//     When the menu is open, swap() captures a frozen game frame once and then
//     draws the menu overlay (background + registered menu drawer) before
//     presenting. This keeps all in-game drawing on the emulation thread.
//
// The device uses the same model with EGL (core/VidExt.cpp).
#include "core/VidExt.h"

#include <functional>
#include <map>
#include <string>
#include <vector>

#include <SDL.h>

#include <GLES2/gl2.h>

#include "ui/Renderer.h"
#include "util/Log.h"
#include "util/Platform.h"

namespace n64ui {

class HostVidExt : public VidExt {
 public:
  HostVidExt() { s_self = this; }
  ~HostVidExt() override { shutdown(); if (s_self == this) s_self = nullptr; }

  bool init(int width, int height) override {
    // A/B test mode: let the CORE create the window/context with its own
    // built-in vidext (stock behavior) instead of ours.
    if (getenv("N64UI_CORE_VIDEXT")) return true;
    if (SDL_InitSubSystem(SDL_INIT_VIDEO) != 0) {
      LOG_ERROR("SDL video init: %s", SDL_GetError());
      return false;
    }
    m_winW = width;
    m_winH = height;
    m_autoTest = getenv("N64UI_AUTOTEST") != nullptr;
    // No window/context yet: create it at the first SetVideoMode, exactly
    // like the stock core's vidext (the Brick's display needs the window
    // created at that point). The UI renderer inits then too.
    LOG_INFO("host vid: deferred context (%dx%d)", (int)width, (int)height);
    return true;
  }

  void shutdown() override {
    releaseCurrent();
    if (m_ctx) SDL_GL_DeleteContext(m_ctx);
    if (m_win) SDL_DestroyWindow(m_win);
    m_ctx = nullptr;
    m_win = nullptr;
  }

  void getFunctions(m64p_video_extension_functions* out) override {
    out->Functions = 17;
    out->VidExtFuncInit = &cbInit;
    out->VidExtFuncQuit = &cbQuit;
    out->VidExtFuncListModes = &cbListModes;
    out->VidExtFuncListRates = &cbListRates;
    out->VidExtFuncSetMode = &cbSetMode;
    out->VidExtFuncSetModeWithRate = &cbSetModeWithRate;
    out->VidExtFuncGLGetProc = &cbGetProc;
    out->VidExtFuncGLSetAttr = &cbSetAttr;
    out->VidExtFuncGLGetAttr = &cbGetAttr;
    out->VidExtFuncGLSwapBuf = &cbSwap;
    out->VidExtFuncSetCaption = &cbSetCaption;
    out->VidExtFuncToggleFS = &cbToggleFS;
    out->VidExtFuncResizeWindow = &cbResizeWindow;
    out->VidExtFuncGLGetDefaultFramebuffer = &cbDefaultFb;
    out->VidExtFuncInitWithRenderMode = &cbInitWithRenderMode;
    out->VidExtFuncVKGetSurface = &cbVkSurface;
    out->VidExtFuncVKGetInstanceExtensions = &cbVkExts;
  }

  bool makeCurrent() override {
    if (!m_win || !m_ctx) return false;
    if (SDL_GL_MakeCurrent(m_win, m_ctx) != 0) {
      LOG_DEBUG("makeCurrent blocked: %s", SDL_GetError());
      return false;
    }
    m_ctxCurrent = true;
    return true;
  }

  void releaseCurrent() override {
    if (m_ctx && m_ctxCurrent) {
      SDL_GL_MakeCurrent(m_win, nullptr);
      m_ctxCurrent = false;
    }
  }

  void swap() override {
    if (m_win) SDL_GL_SwapWindow(m_win);
  }

  void setMenuVisible(bool visible) override {
    m_menu = visible;
  }

  bool menuVisible() const override { return m_menu; }

  void setRenderer(Renderer* renderer) override { m_renderer = renderer; }

  void setMenuDrawer(MenuDrawer drawer) override { m_drawer = std::move(drawer); }

  void menuChanged() override {}

  void setAutoCallback(std::function<void(int)> cb) override {
    m_autoCb = std::move(cb);
  }

  // Dump the current back buffer as a 24-bit BMP (device + host).
  void captureBmp(const std::string& path) {
    if (m_width <= 0 || m_height <= 0) return;
    std::vector<unsigned char> px((size_t)m_width * m_height * 4);
    glReadPixels(0, 0, m_width, m_height, GL_RGBA, GL_UNSIGNED_BYTE, px.data());
    int w = m_width, h = m_height;
    int rowSize = ((w * 3 + 3) / 4) * 4;
    int dataSize = rowSize * h;
    int fileSize = 54 + dataSize;
    std::vector<unsigned char> bmp((size_t)fileSize, 0);
    bmp[0] = 'B'; bmp[1] = 'M';
    bmp[2] = fileSize; bmp[3] = fileSize >> 8; bmp[4] = fileSize >> 16;
    bmp[5] = fileSize >> 24;
    bmp[10] = 54;
    bmp[14] = 40;
    bmp[18] = w; bmp[19] = w >> 8; bmp[20] = w >> 16; bmp[21] = w >> 24;
    bmp[22] = h; bmp[23] = h >> 8; bmp[24] = h >> 16; bmp[25] = h >> 24;
    bmp[26] = 1; bmp[28] = 24;
    size_t off = 54;
    for (int y = h - 1; y >= 0; --y) {
      for (int x = 0; x < w; ++x) {
        size_t i = ((size_t)y * w + x) * 4;
        bmp[off++] = px[i + 2]; bmp[off++] = px[i + 1]; bmp[off++] = px[i];
      }
      off += rowSize - w * 3;
    }
    FILE* f = fopen((path + ".bmp").c_str(), "wb");
    if (f) { fwrite(bmp.data(), 1, bmp.size(), f); fclose(f); }
    LOG_INFO("autotest: captured %s.bmp (%dx%d)", path.c_str(), w, h);
  }

  void presentMenu() override {
    // The menu is drawn from the swap path (emulation thread); nothing to do
    // from the main thread.
  }

  void sessionEnd() override {
    // Game over: back to the launcher; the context goes back to the main
    // thread (dropped here, re-grabbed for the launcher).
    releaseCurrent();
    if (m_win) {
      SDL_SetWindowSize(m_win, Platform::screenWidth(), Platform::screenHeight());
    }
  }

  void raiseWindow() override {
    if (m_win) SDL_RaiseWindow(m_win);
  }

 private:
  static HostVidExt& self() { return *s_self; }
  static HostVidExt* s_self;

  // --- 17 VidExt callbacks (m64p_vidext.h, core 2.6.0) ---

  // m64p_GLattr -> SDL_GLattr translation (the enums don't line up; the core
  // uses the same table in its own vidext.c). SDL2 has no SWAP_CONTROL attr.
  static SDL_GLattr sdlAttr(m64p_GLattr a) {
    switch (a) {
      case M64P_GL_DOUBLEBUFFER: return SDL_GL_DOUBLEBUFFER;
      case M64P_GL_BUFFER_SIZE: return SDL_GL_BUFFER_SIZE;
      case M64P_GL_DEPTH_SIZE: return SDL_GL_DEPTH_SIZE;
      case M64P_GL_RED_SIZE: return SDL_GL_RED_SIZE;
      case M64P_GL_GREEN_SIZE: return SDL_GL_GREEN_SIZE;
      case M64P_GL_BLUE_SIZE: return SDL_GL_BLUE_SIZE;
      case M64P_GL_ALPHA_SIZE: return SDL_GL_ALPHA_SIZE;
      case M64P_GL_MULTISAMPLEBUFFERS: return SDL_GL_MULTISAMPLEBUFFERS;
      case M64P_GL_MULTISAMPLESAMPLES: return SDL_GL_MULTISAMPLESAMPLES;
      case M64P_GL_CONTEXT_MAJOR_VERSION: return SDL_GL_CONTEXT_MAJOR_VERSION;
      case M64P_GL_CONTEXT_MINOR_VERSION: return SDL_GL_CONTEXT_MINOR_VERSION;
      case M64P_GL_CONTEXT_PROFILE_MASK: return SDL_GL_CONTEXT_PROFILE_MASK;
      default: return (SDL_GLattr)-1;
    }
  }

  static m64p_error CALL cbInit(void) { return M64ERR_SUCCESS; }
  static m64p_error CALL cbInitWithRenderMode(m64p_render_mode /*mode*/) {
    return M64ERR_SUCCESS;
  }
  static m64p_error CALL cbQuit(void) {
    self().releaseCurrent();
    return M64ERR_SUCCESS;
  }
  static m64p_error CALL cbListModes(m64p_2d_size* /*sizes*/, int* /*numSizes*/) {
    return M64ERR_UNSUPPORTED;
  }
  static m64p_error CALL cbListRates(m64p_2d_size /*size*/, int* /*rates*/,
                                     int* /*numRates*/) {
    return M64ERR_UNSUPPORTED;
  }
  static m64p_error CALL cbSetMode(int width, int height, int /*bpp*/, int mode,
                                   int /*flags*/) {
    // Runs on the core's emulation thread. The stock core creates the
    // window+context at the first SetVideoMode; do the same (deferred) and
    // init the UI renderer on it.
    HostVidExt& v = self();
    if (!v.m_win) {
      v.createContext(width, height);
      if (v.m_renderer && !v.m_renderer->inited())
        v.m_renderer->init(width, height);
    }
    if (!v.makeCurrent()) return M64ERR_SYSTEM_FAIL;
    if (v.m_win) SDL_SetWindowSize(v.m_win, width, height);
    v.m_width = width;
    v.m_height = height;
    if (v.m_renderer) v.m_renderer->resize(width, height);
    v.setFullscreen(mode == M64VIDEO_FULLSCREEN);
    return M64ERR_SUCCESS;
  }
  static m64p_error CALL cbSetModeWithRate(int width, int height, int bpp,
                                           int /*rate*/, int mode, int flags) {
    return cbSetMode(width, height, bpp, mode, flags);
  }
  static m64p_error CALL cbResizeWindow(int width, int height) {
    return cbSetMode(width, height, 32, self().m_fullscreen ? 3 : 2, 0);
  }
  static m64p_function CALL cbGetProc(const char* symbol) {
    return (m64p_function)SDL_GL_GetProcAddress(symbol);
  }
  static m64p_error CALL cbSetAttr(m64p_GLattr attr, int value) {
    // Record what the plugin asked for so cbGetAttr can report it. The
    // context is created eagerly with equal-or-better settings (e.g. the
    // driver may provide a 32-bit depth buffer when 16 was requested).
    self().m_reqAttrs[attr] = value;
    return M64ERR_SUCCESS;
  }
  static m64p_error CALL cbGetAttr(m64p_GLattr attr, int* value) {
    if (!self().m_ctx) return M64ERR_INVALID_STATE;
    auto it = self().m_reqAttrs.find(attr);
    if (it != self().m_reqAttrs.end()) {
      *value = it->second;
      return M64ERR_SUCCESS;
    }
    SDL_GLattr sdl = sdlAttr(attr);
    if (sdl == (SDL_GLattr)-1) return M64ERR_INVALID_STATE;
    return SDL_GL_GetAttribute(sdl, value) == 0 ? M64ERR_SUCCESS
                                                : M64ERR_INPUT_ASSERT;
  }
  static m64p_error CALL cbSwap(void) {
    // Runs on the emulation thread. Called by the core even while paused (once
    // at pause entry). While the menu is open the main thread repaints it, so
    // release the context at the end and re-grab here each call.
    HostVidExt& v = self();
    if (!v.m_ctxCurrent && !v.makeCurrent()) {
      LOG_WARN("cbSwap: cannot make current");
      return M64ERR_SYSTEM_FAIL;
    }
    if (v.m_menu && v.m_win) {
      if (v.m_renderer) {
        // Direct menu draw (the core is paused, so this runs at pause entry
        // and on frame-advance repaints only — no per-frame cost).
        v.m_renderer->resize(v.m_width, v.m_height);
        v.m_renderer->beginFrame(true);
        if (v.m_drawer) v.m_drawer(*v.m_renderer);
        v.m_renderer->endFrame();
      } else {
        LOG_WARN("cbSwap: menu open but no renderer");
      }
    }
    // Autonomous test mode: the back buffer right now is what is about to be
    // presented. Capture it at fixed swap counts and drive the menu via the
    // registered callback.
    if (v.m_autoTest && !v.m_autoDone) {
      if (++v.m_autoCount == 300) { v.captureBmp("/tmp/ss1"); v.m_autoCb(1); }
      else if (v.m_autoCount == 360) v.m_autoCb(4);  // navigate down
      else if (v.m_autoCount == 380) v.m_autoCb(5);  // navigate down again
      else if (v.m_autoCount == 420) v.captureBmp("/tmp/ss2");
      else if (v.m_autoCount == 440) v.captureBmp("/tmp/ss2b");
      else if (v.m_autoCount == 480) v.m_autoCb(2);
      else if (v.m_autoCount == 600) {
        v.captureBmp("/tmp/ss3");
        v.m_autoDone = true;
        v.m_autoCb(3);
      }
    }
    if (v.m_win) SDL_GL_SwapWindow(v.m_win);
    // The context stays current on the emulation thread for the whole
    // session: releasing it here breaks the next plugin frame on the Brick's
    // EGL (the plugin renders before the next swap can re-grab it). Menu
    // repaints are driven by M64CMD_ADVANCE_FRAME, also on this thread.
    return M64ERR_SUCCESS;
  }
  static m64p_error CALL cbSetCaption(const char* title) {
    if (self().m_win) SDL_SetWindowTitle(self().m_win, title);
    return M64ERR_SUCCESS;
  }
  static m64p_error CALL cbToggleFS(void) {
    self().setFullscreen(!self().m_fullscreen);
    return M64ERR_SUCCESS;
  }
  static uint32_t CALL cbDefaultFb(void) { return 0; }
  static m64p_error CALL cbVkSurface(void** /*surface*/, void* /*instance*/) {
    return M64ERR_UNSUPPORTED;
  }
  static m64p_error CALL cbVkExts(const char** [] /*exts*/, uint32_t* /*count*/) {
    return M64ERR_UNSUPPORTED;
  }

  void setFullscreen(bool fs) {
    m_fullscreen = fs;
    if (!m_win) return;
    SDL_SetWindowFullscreen(m_win, fs ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
  }

  // Mirror the stock core's vidext: create the window+context (with the GLES
  // attributes on the device) at the first SetVideoMode.
  void createContext(int width, int height) {
    if (Platform::isDevice()) {
      SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,
                          SDL_GL_CONTEXT_PROFILE_ES);
      SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
      SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
    }
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    m_win = SDL_CreateWindow("n64ui", SDL_WINDOWPOS_CENTERED,
                             SDL_WINDOWPOS_CENTERED, width, height,
                             SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN);
    if (!m_win) {
      LOG_ERROR("createContext: SDL window: %s", SDL_GetError());
      return;
    }
    m_ctx = SDL_GL_CreateContext(m_win);
    if (!m_ctx) {
      LOG_ERROR("createContext: SDL GL context: %s", SDL_GetError());
      return;
    }
    SDL_GL_MakeCurrent(m_win, m_ctx);
    SDL_GL_SetSwapInterval(0);
    m_ctxCurrent = true;
    m_width = width;
    m_height = height;
    const char* ver = (const char*)glGetString(GL_VERSION);
    LOG_INFO("host vid: window %dx%d, GL: %s", (int)width, (int)height,
             ver ? ver : "?");
  }

  SDL_Window* m_win = nullptr;
  SDL_GLContext m_ctx = nullptr;
  int m_winW = 1024;
  int m_winH = 768;
  int m_width = 1024;
  int m_height = 768;
  bool m_fullscreen = false;
  bool m_menu = false;
  bool m_ctxCurrent = false;
  bool m_autoTest = false;
  int m_autoCount = 0;
  bool m_autoDone = false;
  std::function<void(int)> m_autoCb;
  std::map<int, int> m_reqAttrs;  // m64p_GLattr -> requested value
  MenuDrawer m_drawer;
  Renderer* m_renderer = nullptr;  // owned by main.cpp; same GL context
};

HostVidExt* HostVidExt::s_self = nullptr;

// Defined in core/VidExt.cpp (EGL backend, device).
VidExt* createEglVidExt();

VidExt* VidExt::create() {
  // The SDL_GL-based backend handles both host and device: on the Brick,
  // SDL2 creates an EGL/GLES context from the ES profile attributes set in
  // init(). (The standalone EGL path in VidExt.cpp remains for the future.)
  return new HostVidExt();
}

}  // namespace n64ui
