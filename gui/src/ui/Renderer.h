// GLES2 quad renderer: the only drawing primitive the UI uses.
// Interface only; implemented by RendererImpl (Renderer.cpp).
#pragma once

#include <cstdint>
#include <string>

#include <GLES2/gl2.h>

namespace n64ui {

struct Rgba {
  uint8_t r, g, b, a;
  static Rgba rgb(uint8_t r, uint8_t g, uint8_t b) { return {r, g, b, 255}; }
  static Rgba rgba(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    return {r, g, b, a};
  }
};

struct Rect {
  int x = 0, y = 0, w = 0, h = 0;
};

class Renderer {
 public:
  virtual ~Renderer() = default;

  // Must be called while a GL context is current (after VidExt init).
  virtual bool init(int width, int height) = 0;
  virtual void shutdown() = 0;
  // True once the GL objects were created (used for deferred init).
  virtual bool inited() const = 0;

  // Resize the renderer's coordinate space to the current framebuffer size
  // (called when the video plugin sets a video mode, so the overlay matches
  // the real window). Must be called with the GL context current.
  virtual void resize(int width, int height) = 0;

  virtual void beginFrame() = 0;
  virtual void beginFrame(bool clear) = 0;  // clear to black
  virtual void endFrame() = 0;    // flush (actual swap is VidExt's job)

  // Drawing.
  virtual void drawRect(int x, int y, int w, int h, Rgba color) = 0;
  virtual void drawRectOutline(int x, int y, int w, int h, Rgba color) = 0;
  virtual void drawText(int x, int y, const std::string& text, Rgba color) = 0;
  virtual void drawTexture(GLuint tex, int x, int y, int w, int h) = 0;
  virtual void drawTextureSub(GLuint tex, int x, int y, int w, int h, float u0,
                              float v0, float u1, float v1) = 0;

  // Text measurement (for centering): returns width in pixels (8px/char).
  virtual int textWidth(const std::string& text) const = 0;
  // Height of the current font (pixels), including a small line gap.
  virtual int textHeight() const = 0;
  // Font size in pixels for subsequent drawText/textWidth calls.
  virtual void setFontSize(int px) = 0;

  // Video-plugin compatibility mode. Client-array plugins (Rice) break when a
  // GL_ARRAY_BUFFER stays bound (their vertex pointers become VBO offsets) and
  // leave 3D state on that clips the UI; enable this to clean up after draws.
  // Disabled by default (glide64mk2's behavior is untouched).
  virtual void setClientArrayCompat(bool enabled) = 0;

  // Raise/focus the UI window (host only; no-op on device).
  virtual void focusWindow() = 0;

  static Renderer* create();
};

}  // namespace n64ui
