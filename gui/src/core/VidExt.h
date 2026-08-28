// Video extension (VidExt): the frontend implements the 17 m64p_video_extension
// callbacks so the video plugin (glide64mk2) renders into OUR EGL/GLES2
// context. Also owns the frozen-frame capture used by the in-game menu.
#pragma once

#include <functional>

#include "m64p_common.h"
#include "m64p_types.h"
#include <GLES2/gl2.h>

#include "m64p_vidext.h"

namespace n64ui {

// Forward decl; only a pointer to its drawing API is used here so include is
// kept light (Renderer.h pulls in renderer impl).
class Renderer;

class VidExt {
 public:
  virtual ~VidExt() = default;

  // Create the display/context (1024x768 fullscreen on device, SDL window on
  // host) and register the static callbacks into `out` for CoreOverrideVidExt.
  // On the host the window/context are created here (main thread) so the
  // launcher can be drawn before any game starts; the context then hands off
  // to the emulation thread on the first game SetMode.
  virtual bool init(int width, int height) = 0;
  virtual void shutdown() = 0;

  virtual void getFunctions(m64p_video_extension_functions* out) = 0;

  // Make the GL context current on the calling thread. EGL/GLX allow only one
  // current thread at a time: returns false if another thread still holds it
  // (caller retries until the holder releases). After the game starts, the
  // context stays current on the emulation thread; the main thread no longer
  // touches GL.
  virtual bool makeCurrent() = 0;

  // Release the context from the calling thread so another thread can use it.
  virtual void releaseCurrent() = 0;

  // Present the current frame (eglSwapBuffers / SDL_GL_SwapWindow).
  virtual void swap() = 0;

  // Menu overlay support. The UI registers a drawer that the swap path (which
  // runs on the emulation thread and keeps firing while the core is paused)
  // invokes to paint the in-game menu over the frozen frame. The drawer may be
  // null when no screen is open.
  typedef std::function<void(Renderer&)> MenuDrawer;
  virtual void setMenuVisible(bool visible) = 0;
  virtual bool menuVisible() const = 0;
  // Give the VidExt the UI renderer so its swap path can paint the menu. The
  // renderer is created with the same GL context (on the main thread for the
  // launcher, then used by the emulation thread after the context handoff).
  virtual void setRenderer(Renderer* renderer) = 0;
  virtual void setMenuDrawer(MenuDrawer drawer) = 0;
  virtual void presentMenu() = 0;
  // Signal that the menu content changed (navigation etc.): the swap path
  // re-renders the cached menu texture instead of repainting every frame.
  virtual void menuChanged() = 0;

  // Autonomous test hook: called from the swap path (emulation thread) at
  // fixed swap counts when N64UI_AUTOTEST is set (1=open menu, 2=close,
  // 3=exit).
  virtual void setAutoCallback(std::function<void(int)> cb) = 0;

  // The game session ended (M64CMD_EXECUTE returned): release/destroy
  // per-session window state so the next launch starts clean.
  virtual void sessionEnd() = 0;

  // Bring the game window to the front (host only; no-op on device).
  virtual void raiseWindow() = 0;

  static VidExt* create();
};

}  // namespace n64ui
