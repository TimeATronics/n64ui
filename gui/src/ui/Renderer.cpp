// RendererImpl: minimal GLES2 textured-quad renderer shared by device (EGL)
// and host (SDL_GL). One program, one dynamic VBO, a 1x1 white texture for
// solid quads. All text is rendered with SDL_ttf.
#include "ui/Renderer.h"

#include <GLES2/gl2.h>
#include <SDL_ttf.h>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <string>
#include <tuple>
#include <unordered_map>

#include "util/Log.h"
#include "util/Platform.h"

namespace n64ui {

namespace {

// UI coordinates are top-down (y=0 is the top of the screen); flip to NDC.
const char* kVertSrc =
    "attribute vec2 aPos;\n"
    "attribute vec2 aUv;\n"
    "attribute vec4 aColor;\n"
    "uniform vec2 uRes;\n"
    "varying vec2 vUv;\n"
    "varying vec4 vColor;\n"
    "void main() {\n"
    "  vec2 ndc;\n"
    "  ndc.x = aPos.x / uRes.x * 2.0 - 1.0;\n"
    "  ndc.y = 1.0 - aPos.y / uRes.y * 2.0;\n"
    "  gl_Position = vec4(ndc, 0.0, 1.0);\n"
    "  vUv = aUv;\n"
    "  vColor = aColor;\n"
    "}\n";

// Fragment shader, GLSL ES 1.00 (device EGL/GLES2 context).
const char* kFragSrcEs =
    "#version 100\n"
    "precision mediump float;\n"
    "varying vec2 vUv;\n"
    "varying vec4 vColor;\n"
    "uniform sampler2D uTex;\n"
    "void main() {\n"
    "  vec4 t = texture2D(uTex, vUv);\n"
    "  gl_FragColor = vec4(vColor.rgb * t.rgb, vColor.a * t.a);\n"
    "}\n";

// Fragment shader, desktop GLSL 1.20 (host GLX context).
const char* kFragSrcDesktop =
    "#version 120\n"
    "varying vec2 vUv;\n"
    "varying vec4 vColor;\n"
    "uniform sampler2D uTex;\n"
    "void main() {\n"
    "  vec4 t = texture2D(uTex, vUv);\n"
    "  gl_FragColor = vec4(vColor.rgb * t.rgb, vColor.a * t.a);\n"
    "}\n";

// Solid-color fragment shaders: never sample a texture, so a stale/unbound
// texture can never leak into backgrounds or selection bars.
const char* kSolidFragSrcEs =
    "#version 100\n"
    "precision mediump float;\n"
    "varying vec4 vColor;\n"
    "void main() {\n"
    "  gl_FragColor = vColor;\n"
    "}\n";

const char* kSolidFragSrcDesktop =
    "#version 120\n"
    "varying vec4 vColor;\n"
    "void main() {\n"
    "  gl_FragColor = vColor;\n"
    "}\n";

// interleaved: x, y, u, v, r, g, b, a
constexpr int kFloatsPerVert = 8;
constexpr int kVertBytes = kFloatsPerVert * sizeof(float);
constexpr int kMaxVerts = 6 * 256;  // 256 quads per frame

GLuint compileShader(GLenum type, const char* src) {
  GLuint s = glCreateShader(type);
  glShaderSource(s, 1, &src, nullptr);
  glCompileShader(s);
  GLint ok = 0;
  glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
  if (!ok) {
    char log[256] = {0};
    glGetShaderInfoLog(s, sizeof(log), nullptr, log);
    LOG_ERROR("shader compile: %s", log);
    glDeleteShader(s);
    return 0;
  }
  return s;
}

// Cache key for a rendered text texture: text + 4 color bytes + font size.
std::string textKey(const std::string& text, Rgba c, int size) {
  char buf[64];
  snprintf(buf, sizeof(buf), "|%02x%02x%02x%02x|%d", c.r, c.g, c.b, c.a, size);
  return text + buf;
}

}  // namespace

class RendererImpl : public Renderer {
 public:
  bool init(int width, int height) override {
    m_w = width;
    m_h = height;

    const char* ver = (const char*)glGetString(GL_VERSION);
    bool isGles = ver && strstr(ver, "OpenGL ES") != nullptr;
    const char* fragSrc = isGles ? kFragSrcEs : kFragSrcDesktop;

    GLuint vs = compileShader(GL_VERTEX_SHADER, kVertSrc);
    GLuint fs = compileShader(GL_FRAGMENT_SHADER, fragSrc);
    if (!vs || !fs) return false;
    m_prog = glCreateProgram();
    glAttachShader(m_prog, vs);
    glAttachShader(m_prog, fs);
    glLinkProgram(m_prog);
    glDeleteShader(vs);
    glDeleteShader(fs);
    GLint ok = 0;
    glGetProgramiv(m_prog, GL_LINK_STATUS, &ok);
    if (!ok) {
      LOG_ERROR("program link failed");
      return false;
    }
    m_aPos = glGetAttribLocation(m_prog, "aPos");
    m_aUv = glGetAttribLocation(m_prog, "aUv");
    m_aColor = glGetAttribLocation(m_prog, "aColor");
    m_uRes = glGetUniformLocation(m_prog, "uRes");
    m_uTex = glGetUniformLocation(m_prog, "uTex");

    // Solid-color program: draws backgrounds/selection bars without sampling
    // any texture (no state leak possible).
    const char* solidFrag = isGles ? kSolidFragSrcEs : kSolidFragSrcDesktop;
    GLuint svs = compileShader(GL_VERTEX_SHADER, kVertSrc);
    GLuint sfs = compileShader(GL_FRAGMENT_SHADER, solidFrag);
    if (!svs || !sfs) return false;
    m_solidProg = glCreateProgram();
    glAttachShader(m_solidProg, svs);
    glAttachShader(m_solidProg, sfs);
    glLinkProgram(m_solidProg);
    glDeleteShader(svs);
    glDeleteShader(sfs);
    glGetProgramiv(m_solidProg, GL_LINK_STATUS, &ok);
    if (!ok) {
      LOG_ERROR("solid program link failed");
      return false;
    }
    m_sUres = glGetUniformLocation(m_solidProg, "uRes");

    glGenBuffers(1, &m_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferData(GL_ARRAY_BUFFER, kMaxVerts * kVertBytes, nullptr, GL_DYNAMIC_DRAW);

    unsigned char white[4] = {255, 255, 255, 255};
    glGenTextures(1, &m_whiteTex);
    glBindTexture(GL_TEXTURE_2D, m_whiteTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                 white);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    // All text is rendered with SDL_ttf.
    if (TTF_Init() != 0) {
      LOG_ERROR("TTF_Init failed: %s", TTF_GetError());
      return false;
    }
    const char* env = getenv("N64UI_FONT");
    const char* candidates[] = {
        env ? env : "",
        "./font.ttf",  // device payload dir (launch.sh cds there)
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf",
    };
    for (const char* path : candidates) {
      if (!path || !*path) continue;
      m_ttf = TTF_OpenFont(path, m_fontSize);
      if (m_ttf) {
        m_ttfPath = path;
        LOG_INFO("ttf font: %s (%dpx)", path, m_fontSize);
        break;
      }
    }
    if (!m_ttf) {
      LOG_ERROR("no ttf font found (set N64UI_FONT)");
      return false;
    }

    // Deliberately no GL state changes here (no viewport/blend): the video
    // plugin starts right after and must find the context in its pristine
    // state, exactly like the stock frontend. The menu's beginFrame() sets
    // blending/viewport when it actually draws.
    if (m_compat) glBindBuffer(GL_ARRAY_BUFFER, 0);
    LOG_INFO("gles renderer %dx%d", width, height);
    return true;
  }

  bool inited() const override { return m_prog != 0; }

  void resize(int width, int height) override {
    std::lock_guard<std::mutex> lk(m_mtx);
    m_w = width;
    m_h = height;
    if (glGetString(GL_VERSION)) glViewport(0, 0, width, height);
  }

  void shutdown() override {
    for (auto& kv : m_textCache) {
      GLuint tex = std::get<0>(kv.second);
      glDeleteTextures(1, &tex);
    }
    m_textCache.clear();
    if (m_whiteTex) glDeleteTextures(1, &m_whiteTex);
    if (m_vbo) glDeleteBuffers(1, &m_vbo);
    if (m_prog) glDeleteProgram(m_prog);
    if (m_solidProg) glDeleteProgram(m_solidProg);
    m_whiteTex = m_vbo = m_prog = m_solidProg = 0;
    if (m_ttf) {
      TTF_CloseFont(m_ttf);
      m_ttf = nullptr;
    }
    TTF_Quit();
  }

  void beginFrame() override { beginFrame(true); }

  void beginFrame(bool clear) override {
    // Serialize frames: the emu thread draws at pause entry while the main
    // thread repaints navigation, so only one may touch the VBO at a time.
    m_mtx.lock();
    // Remember the video plugin's GL state so we can restore it after the
    // UI draw (the plugin must find its own state on the next frame).
    if (m_compat) {
      m_savedBlend = glIsEnabled(GL_BLEND);
      m_savedCull = glIsEnabled(GL_CULL_FACE);
      m_savedScissor = glIsEnabled(GL_SCISSOR_TEST);
      m_savedDepth = glIsEnabled(GL_DEPTH_TEST);
      m_savedStencil = glIsEnabled(GL_STENCIL_TEST);
      glGetIntegerv(GL_CURRENT_PROGRAM, &m_savedProgram);
      glGetIntegerv(GL_BLEND_SRC_RGB, &m_savedBlendSrc);
      glGetIntegerv(GL_BLEND_DST_RGB, &m_savedBlendDst);
      glGetIntegerv(GL_VIEWPORT, m_savedViewport);
      glGetIntegerv(GL_ACTIVE_TEXTURE, &m_savedActiveTex);
      for (int u = 0; u < kSavedTexUnits; ++u) {
        glActiveTexture(GL_TEXTURE0 + u);
        glGetIntegerv(GL_TEXTURE_BINDING_2D, &m_savedTex[u]);
      }
      glActiveTexture((GLenum)m_savedActiveTex);
      glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &m_savedArrayBuf);
      glGetIntegerv(GL_SCISSOR_BOX, m_savedScissorBox);
      glGetIntegerv(GL_FRAMEBUFFER_BINDING, &m_savedFbo);
      // Note: we don't bind framebuffer 0 here -- the caller decides the draw
      // target (default framebuffer for the menu blit, the menu FBO for the
      // cached render). We only save/restore the binding.
      // Save the vertex-attribute array state (the plugin may cache which
      // arrays are enabled with its pointers; our flush disables ours).
      for (int i = 0; i < kSavedAttribs; ++i) {
        glGetVertexAttribiv(i, GL_VERTEX_ATTRIB_ARRAY_ENABLED,
                            &m_savedAttr[i].enabled);
        glGetVertexAttribiv(i, GL_VERTEX_ATTRIB_ARRAY_SIZE,
                            &m_savedAttr[i].size);
        glGetVertexAttribiv(i, GL_VERTEX_ATTRIB_ARRAY_TYPE,
                            &m_savedAttr[i].type);
        glGetVertexAttribiv(i, GL_VERTEX_ATTRIB_ARRAY_STRIDE,
                            &m_savedAttr[i].stride);
        glGetVertexAttribiv(i, GL_VERTEX_ATTRIB_ARRAY_NORMALIZED,
                            &m_savedAttr[i].normalized);
        glGetVertexAttribPointerv(i, GL_VERTEX_ATTRIB_ARRAY_POINTER,
                                  &m_savedAttr[i].pointer);
      }
      // The UI must never draw into a foreign FBO (e.g. the video plugin's
      // render target); bind the default framebuffer for our 2D draws.
      glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }
    if (clear) {
      glClearColor(0.f, 0.f, 0.f, 1.f);
      glClear(GL_COLOR_BUFFER_BIT);
    }
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    if (m_compat) {
      glDisable(GL_CULL_FACE);
      glDisable(GL_SCISSOR_TEST);
      glDisable(GL_DEPTH_TEST);
      glDisable(GL_STENCIL_TEST);
    }
    m_quads = 0;
  }

  void endFrame() override {
    flushQuads();
    glFlush();
    // Restore the video plugin's GL state that we changed for the 2D UI.
    if (m_compat) {
      setEnabled(GL_BLEND, m_savedBlend);
      setEnabled(GL_CULL_FACE, m_savedCull);
      setEnabled(GL_SCISSOR_TEST, m_savedScissor);
      setEnabled(GL_DEPTH_TEST, m_savedDepth);
      setEnabled(GL_STENCIL_TEST, m_savedStencil);
      glBlendFunc((GLenum)m_savedBlendSrc, (GLenum)m_savedBlendDst);
      // The plugin caches its program/combiner setup; restore ours so it
      // keeps drawing with its own shader.
      if (m_savedProgram) glUseProgram((GLuint)m_savedProgram);
      glViewport(m_savedViewport[0], m_savedViewport[1], m_savedViewport[2],
                 m_savedViewport[3]);
      glScissor(m_savedScissorBox[0], m_savedScissorBox[1],
                m_savedScissorBox[2], m_savedScissorBox[3]);
      // Restore texture unit bindings and the array buffer.
      for (int u = 0; u < kSavedTexUnits; ++u) {
        glActiveTexture(GL_TEXTURE0 + u);
        glBindTexture(GL_TEXTURE_2D, (GLuint)m_savedTex[u]);
      }
      glActiveTexture((GLenum)m_savedActiveTex);
      glBindBuffer(GL_ARRAY_BUFFER, (GLuint)m_savedArrayBuf);
      glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)m_savedFbo);
      // Restore the vertex-attribute array state.
      for (int i = 0; i < kSavedAttribs; ++i) {
        if (m_savedAttr[i].enabled)
          glEnableVertexAttribArray(i);
        else
          glDisableVertexAttribArray(i);
        glVertexAttribPointer(
            i, m_savedAttr[i].size, (GLenum)m_savedAttr[i].type,
            m_savedAttr[i].normalized ? GL_TRUE : GL_FALSE,
            m_savedAttr[i].stride, m_savedAttr[i].pointer);
      }
    }
    m_mtx.unlock();
  }

  static void setEnabled(GLenum cap, bool on) {
    if (on)
      glEnable(cap);
    else
      glDisable(cap);
  }

  void drawRect(int x, int y, int w, int h, Rgba color) override {
    drawQuad(x, y, w, h, m_whiteTex, 0.f, 0.f, 1.f, 1.f, color);
  }

  void drawRectOutline(int x, int y, int w, int h, Rgba color) override {
    drawRect(x, y, w, 1, color);
    drawRect(x, y + h - 1, w, 1, color);
    drawRect(x, y, 1, h, color);
    drawRect(x + w - 1, y, 1, h, color);
  }

  void drawText(int x, int y, const std::string& text, Rgba color) override {
    if (text.empty()) return;
    drawTtf(x, y, text, color);
  }

  void drawTexture(GLuint tex, int x, int y, int w, int h) override {
    drawTextureSub(tex, x, y, w, h, 0.f, 0.f, 1.f, 1.f);
  }

  void drawTextureSub(GLuint tex, int x, int y, int w, int h, float u0, float v0,
                      float u1, float v1) override {
    drawQuad(x, y, w, h, tex, u0, v0, u1, v1, Rgba::rgb(255, 255, 255));
  }

  int textWidth(const std::string& text) const override {
    int w = 0, h = 0;
    if (TTF_SizeUTF8(m_ttf, text.c_str(), &w, &h) == 0) return w;
    return 0;
  }

  int textHeight() const override { return TTF_FontHeight(m_ttf) + 2; }

  void setFontSize(int px) override {
    if (px <= 0) px = 16;
    if (px == m_fontSize) return;
#if SDL_TTF_MAJOR_VERSION > 2 || \
    (SDL_TTF_MAJOR_VERSION == 2 && SDL_TTF_MINOR_VERSION >= 18)
    if (TTF_SetFontSize(m_ttf, px) != 0) {
      LOG_WARN("TTF_SetFontSize(%d) failed: %s", px, TTF_GetError());
      return;
    }
#else
    // Older SDL_ttf (device sysroot, 2.0.10): reopen the font at the size.
    if (!m_ttfPath.empty()) {
      TTF_Font* f = TTF_OpenFont(m_ttfPath.c_str(), px);
      if (!f) {
        LOG_WARN("TTF_OpenFont(%d) failed: %s", px, TTF_GetError());
        return;
      }
      TTF_CloseFont(m_ttf);
      m_ttf = f;
    }
#endif
    m_fontSize = px;
  }

  void setClientArrayCompat(bool enabled) override {
    m_compat = enabled;
    LOG_INFO("renderer client-array compat: %s", enabled ? "on" : "off");
    // Client-array plugins (Rice) draw with client-memory vertex arrays; a
    // bound GL_ARRAY_BUFFER turns those pointers into VBO offsets. Unbind
    // whatever our init left bound (when a context is current).
    if (m_compat && glGetString(GL_VERSION)) glBindBuffer(GL_ARRAY_BUFFER, 0);
  }

  void focusWindow() override {
    // Single-window design: the VidExt window is the UI window too.
  }

 private:
  void drawTtf(int x, int y, const std::string& text, Rgba color) {
    std::string key = textKey(text, color, m_fontSize);
    auto it = m_textCache.find(key);
    GLuint tex = 0;
    int tw = 0, th = 0;
    if (it != m_textCache.end()) {
      tex = std::get<0>(it->second);
      tw = std::get<1>(it->second);
      th = std::get<2>(it->second);
    } else {
      SDL_Color c = {color.r, color.g, color.b, color.a};
      SDL_Surface* surf = TTF_RenderUTF8_Blended(m_ttf, text.c_str(), c);
      if (!surf) {
        LOG_WARN("TTF render '%s' failed: %s", text.c_str(), TTF_GetError());
        return;
      }
      // Convert to R,G,B,A byte order in memory.
      SDL_Surface* rgba = SDL_ConvertSurfaceFormat(surf, SDL_PIXELFORMAT_ABGR8888, 0);
      SDL_FreeSurface(surf);
      if (!rgba) {
        LOG_WARN("TTF surface convert failed: %s", SDL_GetError());
        return;
      }
      tw = rgba->w;
      th = rgba->h;
      glGenTextures(1, &tex);
      glBindTexture(GL_TEXTURE_2D, tex);
      glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, tw, th, 0, GL_RGBA,
                   GL_UNSIGNED_BYTE, rgba->pixels);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
      SDL_FreeSurface(rgba);
      m_textCache[key] = {tex, tw, th};
    }
    drawTexture(tex, x, y, tw, th);
  }

  void drawQuad(int x, int y, int w, int h, GLuint tex, float u0, float v0,
                float u1, float v1, Rgba color) {
    if (m_quads >= kMaxVerts / 6) {
      flushQuads();
    }
    float* v = &m_verts[m_quads * 6 * kFloatsPerVert];
    float fr = color.r / 255.f, fg = color.g / 255.f, fb = color.b / 255.f,
          fa = color.a / 255.f;
    float x0 = (float)x, y0 = (float)y, x1 = (float)(x + w), y1 = (float)(y + h);
    float quad[6][kFloatsPerVert] = {
        {x0, y0, u0, v0, fr, fg, fb, fa},
        {x1, y0, u1, v0, fr, fg, fb, fa},
        {x1, y1, u1, v1, fr, fg, fb, fa},
        {x0, y0, u0, v0, fr, fg, fb, fa},
        {x1, y1, u1, v1, fr, fg, fb, fa},
        {x0, y1, u0, v1, fr, fg, fb, fa},
    };
    memcpy(v, quad, sizeof(quad));
    m_quadTexs[m_quads] = tex;
    m_quadSolid[m_quads] = (tex == 0 || tex == m_whiteTex);
    m_quads++;
    if (m_quads >= kMaxVerts / 6) flushQuads();
  }

  void flushQuads() {
    if (m_quads == 0) return;
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferData(GL_ARRAY_BUFFER, m_quads * 6 * kVertBytes, m_verts,
                 GL_DYNAMIC_DRAW);
    // Both programs share the same vertex layout; extra enabled arrays that
    // a program does not reference are ignored, so enable everything once.
    glEnableVertexAttribArray(m_aPos);
    glVertexAttribPointer(m_aPos, 2, GL_FLOAT, GL_FALSE, kVertBytes, (void*)0);
    glEnableVertexAttribArray(m_aUv);
    glVertexAttribPointer(m_aUv, 2, GL_FLOAT, GL_FALSE, kVertBytes,
                          (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(m_aColor);
    glVertexAttribPointer(m_aColor, 4, GL_FLOAT, GL_FALSE, kVertBytes,
                          (void*)(4 * sizeof(float)));
    int runStart = 0;
    for (int i = 1; i <= m_quads; ++i) {
      if (i == m_quads || m_quadSolid[i] != m_quadSolid[runStart] ||
          (!m_quadSolid[i] && m_quadTexs[i] != m_quadTexs[runStart])) {
        drawRun(runStart, i);
        runStart = i;
      }
    }
    glDisableVertexAttribArray(m_aPos);
    glDisableVertexAttribArray(m_aUv);
    glDisableVertexAttribArray(m_aColor);
    m_quads = 0;
    // Client-array plugins (Rice) need no texture and no GL_ARRAY_BUFFER
    // bound when they draw; leave the context clean in compat mode only.
    glBindTexture(GL_TEXTURE_2D, 0);
    if (m_compat) glBindBuffer(GL_ARRAY_BUFFER, 0);
  }

  void drawRun(int start, int end) {
    int count = (end - start) * 6;
    if (m_quadSolid[start]) {
      // Solid quad: the fragment shader never samples a texture, so no stale
      // texture state can leak into backgrounds or selection bars.
      glUseProgram(m_solidProg);
      glUniform2f(m_sUres, (float)m_w, (float)m_h);
      glDrawArrays(GL_TRIANGLES, start * 6, count);
    } else {
      glUseProgram(m_prog);
      glUniform2f(m_uRes, (float)m_w, (float)m_h);
      glUniform1i(m_uTex, 0);
      glActiveTexture(GL_TEXTURE0);
      glBindTexture(GL_TEXTURE_2D, m_quadTexs[start]);
      glDrawArrays(GL_TRIANGLES, start * 6, count);
    }
  }

  int m_w = 0;
  int m_h = 0;

  GLuint m_prog = 0;
  GLuint m_solidProg = 0;
  GLint m_aPos = -1, m_aUv = -1, m_aColor = -1, m_uRes = -1, m_uTex = -1;
  GLint m_sUres = -1;
  GLuint m_vbo = 0;
  GLuint m_whiteTex = 0;
  int m_fontSize = 16;
  TTF_Font* m_ttf = nullptr;
  std::string m_ttfPath;
  std::unordered_map<std::string, std::tuple<GLuint, int, int>> m_textCache;
  float m_verts[kMaxVerts * kFloatsPerVert];
  GLuint m_quadTexs[kMaxVerts / 6];
  bool m_quadSolid[kMaxVerts / 6];
  int m_quads = 0;
  bool m_compat = false;
  bool m_savedBlend = false, m_savedCull = false, m_savedScissor = false;
  bool m_savedDepth = false, m_savedStencil = false;
  GLint m_savedProgram = 0, m_savedBlendSrc = 0, m_savedBlendDst = 0;
  GLint m_savedViewport[4] = {0, 0, 0, 0};
  GLint m_savedScissorBox[4] = {0, 0, 0, 0};
  GLint m_savedActiveTex = 0, m_savedArrayBuf = 0, m_savedFbo = 0;
  static constexpr int kSavedTexUnits = 8;
  GLint m_savedTex[kSavedTexUnits] = {0};
  static constexpr int kSavedAttribs = 8;
  struct AttrState {
    GLint enabled = 0, size = 4, type = 0, stride = 0, normalized = 0;
    void* pointer = nullptr;
  };
  AttrState m_savedAttr[kSavedAttribs];
  std::mutex m_mtx;
};

Renderer* Renderer::create() { return new RendererImpl(); }

}  // namespace n64ui
