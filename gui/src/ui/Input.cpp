// InputImpl: state-polling input (no event consumption, so the input-sdl
// plugin's own SDL event pump is undisturbed).
//  - Device: joystick "TRIMUI Player1": A=1 B=0 X=3 Y=2 L1=4 R1=5 Select=6
//    Start=7 Menu=8, dpad = hat 0; power/volume keys on the keyboard.
//  - Host: keyboard (arrows + Enter/Esc/Backspace).
// Edge detection on poll; dpad auto-repeat (400ms/120ms) while held.
#include "ui/Input.h"

#include <algorithm>
#include <string>

#include "util/Log.h"
#include "util/Platform.h"
#include "util/Str.h"

namespace n64ui {

namespace {
constexpr int kRepeatDelayMs = 400;
constexpr int kRepeatRateMs = 120;
}  // namespace

class InputImpl : public Input {
 public:
  bool init() override {
    SDL_Init(SDL_INIT_JOYSTICK | SDL_INIT_GAMECONTROLLER);
    // Joystick events are NOT enabled by default in this SDL build; without
    // them the event queue never sees button/hat/axis changes (capture and
    // fast taps rely on it). Same as refs/music_player input.go.
    SDL_JoystickEventState(SDL_ENABLE);
    m_debug = getenv("N64UI_DEBUG_INPUT") != nullptr;
    if (Platform::isDevice()) {
      m_joy = SDL_JoystickOpen(0);
      if (m_joy) {
        m_joyIndex = SDL_JoystickInstanceID(m_joy);
        LOG_INFO("joystick: %s (%d buttons, %d axes, %d hats)",
                 SDL_JoystickName(m_joy), SDL_JoystickNumButtons(m_joy),
                 SDL_JoystickNumAxes(m_joy), SDL_JoystickNumHats(m_joy));
      } else {
        LOG_WARN("no joystick 0: %s", SDL_GetError());
      }
    }
    m_prev = readState();
    return true;
  }

  void debugEvents() {
    if (!m_debug) return;
    SDL_Event ev;
    while (SDL_PollEvent(&ev)) {
      switch (ev.type) {
        case SDL_JOYBUTTONDOWN:
        case SDL_JOYBUTTONUP:
          LOG_INFO("input debug: ev btn%d %s", ev.jbutton.button,
                   ev.jbutton.state == SDL_PRESSED ? "down" : "up");
          break;
        case SDL_JOYHATMOTION:
          LOG_INFO("input debug: ev hat%d=%d", ev.jhat.hat, ev.jhat.value);
          break;
        case SDL_JOYAXISMOTION:
          LOG_INFO("input debug: ev axis%d=%d", ev.jaxis.axis, ev.jaxis.value);
          break;
        case SDL_KEYDOWN:
          LOG_INFO("input debug: ev key sc=%d", ev.key.keysym.scancode);
          break;
        default:
          break;
      }
    }
  }

  void debugDump(const char* who) {
    if (!m_debug || !m_joy) return;
    std::string s = who;
    int hats = SDL_JoystickNumHats(m_joy);
    for (int h = 0; h < hats; ++h)
      s += strFormat(" hat%d=%d", h, SDL_JoystickGetHat(m_joy, h));
    int btns = SDL_JoystickNumButtons(m_joy);
    for (int b = 0; b < btns; ++b) {
      if (SDL_JoystickGetButton(m_joy, b)) s += strFormat(" btn%d", b);
    }
    int axes = SDL_JoystickNumAxes(m_joy);
    for (int a = 0; a < axes; ++a)
      s += strFormat(" ax%d=%d", a, SDL_JoystickGetAxis(m_joy, a));
    LOG_INFO("input debug: %s", s.c_str());
  }

  void shutdown() override {
    if (m_joy) SDL_JoystickClose(m_joy);
    m_joy = nullptr;
  }

  void resync() override {
    // Re-baseline edges for keys held through the transition, but KEEP
    // m_lastMenuAt: resetting it would let a key-bounce re-trigger the menu
    // within the debounce window (the "press Escape twice" symptom).
    m_prev = readState();
  }

  void waitForRelease(int timeoutMs) override {
    Uint32 deadline = SDL_GetTicks() + timeoutMs;
    while (SDL_GetTicks() < deadline) {
      State s = readState();
      if (!s.a && !s.b && !s.up && !s.down && !s.left && !s.right && !s.menu)
        return;
      SDL_Delay(10);
    }
  }

  bool capturePoll(std::string& out) override {
    // Wait until every input is released before accepting a press (the key
    // used to enter capture is usually still down). Computed from STATE so a
    // stuck hardware key can't deadlock the wait; only real releases count.
    CaptureState st = readCaptureState();
    if (st.cancel) return false;  // Escape/Backspace
    if (m_captureWaitRelease) {
      if (st.empty) m_captureWaitRelease = false;
      return true;  // keep waiting
    }
    // 1) Event queue first (flushed at beginCapture): fires only on real
    //    transitions. (The input-sdl plugin never drains the queue.)
    SDL_Event ev;
    while (SDL_PollEvent(&ev)) {
      switch (ev.type) {
        case SDL_KEYDOWN: {
          if (ev.key.keysym.scancode == SDL_SCANCODE_ESCAPE ||
              ev.key.keysym.scancode == SDL_SCANCODE_BACKSPACE)
            return false;  // cancel
          out = "key(" + std::to_string((int)ev.key.keysym.scancode) + ")";
          m_captureWaitRelease = true;
          return true;
        }
        case SDL_JOYBUTTONDOWN: {
          if (ev.jbutton.which == m_joyIndex) {
            out = "button(" + std::to_string((int)ev.jbutton.button) + ")";
            m_captureWaitRelease = true;
            return true;
          }
          break;
        }
        case SDL_JOYHATMOTION: {
          if (ev.jhat.which == m_joyIndex && ev.jhat.value != SDL_HAT_CENTERED) {
            const char* dir = (ev.jhat.value & SDL_HAT_UP)    ? "Up"
                              : (ev.jhat.value & SDL_HAT_DOWN)  ? "Down"
                              : (ev.jhat.value & SDL_HAT_LEFT)  ? "Left"
                                                                : "Right";
            out = "hat(0 " + std::string(dir) + ")";
            m_captureWaitRelease = true;
            return true;
          }
          break;
        }
        case SDL_JOYAXISMOTION: {
          if (ev.jaxis.which == m_joyIndex &&
              (ev.jaxis.value > 16384 || ev.jaxis.value < -16384)) {
            out = "axis(" + std::to_string((int)ev.jaxis.axis) +
                  (ev.jaxis.value > 0 ? "+" : "-") + ")";
            m_captureWaitRelease = true;
            return true;
          }
          break;
        }
        default:
          break;
      }
    }
    // 2) State fallback (the Brick's driver generates no joystick events, so
    //    this is the primary path there): a NEWLY pressed input emits.
    if (st.empty || st == m_capturePrev) return true;
    m_capturePrev = st;
    if (st.key >= 0) {
      out = "key(" + std::to_string(st.key) + ")";
    } else if (st.button >= 0) {
      out = "button(" + std::to_string(st.button) + ")";
    } else if (st.hat >= 0) {
      const char* dir = (st.hatDir & SDL_HAT_UP)    ? "Up"
                        : (st.hatDir & SDL_HAT_DOWN)  ? "Down"
                        : (st.hatDir & SDL_HAT_LEFT)  ? "Left"
                                                      : "Right";
      out = "hat(0 " + std::string(dir) + ")";
    } else if (st.axis >= 0) {
      out = "axis(" + std::to_string(st.axis) +
            (st.axisSign > 0 ? "+" : "-") + ")";
    }
    m_captureWaitRelease = true;  // re-arm for the next mapping
    return true;
  }

  void beginCapture() override {
    // Wait for every input to be released before accepting a press.
    m_captureWaitRelease = true;
    m_capturePrev = {};
    // Baseline the axes so a trigger held at capture start isn't a "push".
    if (m_joy) {
      int axes = SDL_JoystickNumAxes(m_joy);
      for (int a = 0; a < axes && a < 32; ++a)
        m_axisPrev[a] = SDL_JoystickGetAxis(m_joy, a);
    }
    // Drop stale events (the press that armed capture, etc.) so the next
    // real press is the first thing seen.
    SDL_FlushEvents(SDL_FIRSTEVENT, SDL_LASTEVENT);
  }

  // Sleep only; all input is read via pollActions (state polling).
  bool waitEvent(int timeoutMs) override {
    SDL_Event ev;
    return SDL_WaitEventTimeout(&ev, timeoutMs) == 1;
  }

  std::vector<Action> pollActions() override {
    std::vector<Action> out;
    State cur = readState();
    debugDump("poll");
    debugEvents();

    // Edge detection: rising edges emit an action.
    emitEdges(cur, out);

    // Dpad auto-repeat while held.
    Uint32 now = SDL_GetTicks();
    if (cur.dpadHeld()) {
      if (!m_prev.dpadHeld()) {
        m_repeatStart = now;
      } else if (now - m_repeatStart >= kRepeatDelayMs &&
                 now - m_lastRepeat >= kRepeatRateMs) {
        m_lastRepeat = now;
        for (ActionType t : {ActionType::Up, ActionType::Down, ActionType::Left,
                             ActionType::Right}) {
          if (held(cur, t)) out.push_back({t, true});
        }
      }
    }
    m_prev = cur;
    return out;
  }

 private:
  struct State {
    bool up = false, down = false, left = false, right = false;
    bool a = false, b = false, x = false, y = false;
    bool l1 = false, r1 = false, sel = false, start = false, menu = false;
    bool power = false, volUp = false, volDown = false;

    bool dpadHeld() const { return up || down || left || right; }
  };

  static bool held(const State& s, ActionType t) {
    switch (t) {
      case ActionType::Up: return s.up;
      case ActionType::Down: return s.down;
      case ActionType::Left: return s.left;
      case ActionType::Right: return s.right;
      default: return false;
    }
  }

  State readState() {
    State s;
    const Uint8* ks = SDL_GetKeyboardState(nullptr);
    if (m_joy) {
      Uint8 hat = SDL_JoystickGetHat(m_joy, 0);
      s.up = hat & SDL_HAT_UP;
      s.down = hat & SDL_HAT_DOWN;
      s.left = hat & SDL_HAT_LEFT;
      s.right = hat & SDL_HAT_RIGHT;
      s.a = SDL_JoystickGetButton(m_joy, 1);
      s.b = SDL_JoystickGetButton(m_joy, 0);
      s.x = SDL_JoystickGetButton(m_joy, 3);
      s.y = SDL_JoystickGetButton(m_joy, 2);
      s.l1 = SDL_JoystickGetButton(m_joy, 4);
      s.r1 = SDL_JoystickGetButton(m_joy, 5);
      s.sel = SDL_JoystickGetButton(m_joy, 6);
      s.start = SDL_JoystickGetButton(m_joy, 7);
      s.menu = SDL_JoystickGetButton(m_joy, 8);
    } else if (ks) {
      s.up = ks[SDL_SCANCODE_UP];
      s.down = ks[SDL_SCANCODE_DOWN];
      s.left = ks[SDL_SCANCODE_LEFT];
      s.right = ks[SDL_SCANCODE_RIGHT];
      s.a = ks[SDL_SCANCODE_RETURN];
      s.b = ks[SDL_SCANCODE_BACKSPACE];
      s.x = ks[SDL_SCANCODE_X];
      s.y = ks[SDL_SCANCODE_Y];
      s.l1 = ks[SDL_SCANCODE_Q];
      s.r1 = ks[SDL_SCANCODE_E];
      s.sel = ks[SDL_SCANCODE_TAB];
      s.start = ks[SDL_SCANCODE_SPACE];
      s.menu = ks[SDL_SCANCODE_ESCAPE];
    }
    if (ks) {
      s.power = ks[SDL_SCANCODE_POWER];
      s.volUp = ks[SDL_SCANCODE_KP_PLUS] || ks[SDL_SCANCODE_EQUALS];
      s.volDown = ks[SDL_SCANCODE_KP_MINUS] || ks[SDL_SCANCODE_MINUS];
    }
    return s;
  }

  // One (or zero) distinct input seen during a capture poll.
  struct CaptureState {
    int key = -1;
    int button = -1;
    int hat = -1;
    Uint8 hatDir = 0;
    int axis = -1;
    int axisSign = 0;
    bool cancel = false;
    bool empty = true;

    bool operator==(const CaptureState& o) const {
      return key == o.key && button == o.button && hat == o.hat &&
             hatDir == o.hatDir && axis == o.axis && axisSign == o.axisSign;
    }
  };

  CaptureState readCaptureState() {
    CaptureState st;
    // Axis pushes are detected by DELTA from the last poll's value, not by
    // an absolute center threshold: the Brick's triggers (L2=axis 2, R2=axis
    // 5) REST at -32768 and press to +32767, so they never cross zero-based
    // center. A real press jumps the axis (delta > 20000); rest jitter never
    // does, so nothing self-captures.
    if (m_joy) {
      // Axis pushes are detected by DELTA from the last poll's value, not by
      // an absolute center threshold: the Brick's triggers (L2=axis 2, R2=axis
      // 5) REST at -32768 and press to +32767, so they never cross zero-based
      // center. A real press jumps the axis (delta > 20000); rest jitter never
      // does, so nothing self-captures. Axes are excluded from the
      // wait-release check (a resting trigger must not block button mapping);
      // deltas are absorbed into the baseline while waiting so the release
      // jump doesn't look like a push.
      if (!m_captureWaitRelease) {
        int axes = SDL_JoystickNumAxes(m_joy);
        for (int a = 0; a < axes && a < 32; ++a) {
          Sint16 v = SDL_JoystickGetAxis(m_joy, a);
          int delta = abs((int)v - m_axisPrev[a]);
          m_axisPrev[a] = v;
          if (delta > 20000) {
            st.axis = a;
            st.axisSign = v > 0 ? 1 : -1;
            st.empty = false;
            return st;
          }
        }
      } else {
        int axes = SDL_JoystickNumAxes(m_joy);
        for (int a = 0; a < axes && a < 32; ++a)
          m_axisPrev[a] = SDL_JoystickGetAxis(m_joy, a);
      }
    }
    const Uint8* ks = SDL_GetKeyboardState(nullptr);
    if (ks) {
      if (ks[SDL_SCANCODE_ESCAPE] || ks[SDL_SCANCODE_BACKSPACE]) {
        st.cancel = true;
        st.empty = false;
        return st;
      }
      for (int sc = SDL_SCANCODE_A; sc < SDL_NUM_SCANCODES; ++sc) {
        if (!ks[sc]) continue;
        // Hardware keys (power/menu/volume...) are not mappable and can be
        // reported held forever by the driver; skip them so a stuck key
        // can't block capture.
        if (sc == SDL_SCANCODE_POWER || sc == SDL_SCANCODE_MENU ||
            sc == SDL_SCANCODE_VOLUMEUP || sc == SDL_SCANCODE_VOLUMEDOWN ||
            sc == SDL_SCANCODE_MUTE)
          continue;
        st.key = sc;
        st.empty = false;
        return st;  // one key at a time
      }
    }
    if (m_joy) {
      int hats = SDL_JoystickNumHats(m_joy);
      for (int h = 0; h < hats; ++h) {
        Uint8 v = SDL_JoystickGetHat(m_joy, h);
        if (v != SDL_HAT_CENTERED) {
          st.hat = h;
          st.hatDir = v;
          st.empty = false;
          return st;
        }
      }
      int btns = SDL_JoystickNumButtons(m_joy);
      for (int b = 0; b < btns; ++b) {
        if (SDL_JoystickGetButton(m_joy, b)) {
          st.button = b;
          st.empty = false;
          return st;
        }
      }
    }
    return st;
  }

  void emitEdges(const State& cur, std::vector<Action>& out) {
    Uint32 now = SDL_GetTicks();
    if (cur.menu && !m_prev.menu) {
      // Debounce: focus changes / keymap bounce can flicker the Menu state.
      if (now - m_lastMenuAt > kRepeatDelayMs) {
        out.push_back({ActionType::Menu, false});
        m_lastMenuAt = now;
      }
    }
    if (cur.up && !m_prev.up) out.push_back({ActionType::Up, false});
    if (cur.down && !m_prev.down) out.push_back({ActionType::Down, false});
    if (cur.left && !m_prev.left) out.push_back({ActionType::Left, false});
    if (cur.right && !m_prev.right) out.push_back({ActionType::Right, false});
    if (cur.a && !m_prev.a) out.push_back({ActionType::A, false});
    if (cur.b && !m_prev.b) out.push_back({ActionType::B, false});
    if (cur.x && !m_prev.x) out.push_back({ActionType::X, false});
    if (cur.y && !m_prev.y) out.push_back({ActionType::Y, false});
    if (cur.l1 && !m_prev.l1) out.push_back({ActionType::L1, false});
    if (cur.r1 && !m_prev.r1) out.push_back({ActionType::R1, false});
    if (cur.sel && !m_prev.sel) out.push_back({ActionType::Select, false});
    if (cur.start && !m_prev.start) out.push_back({ActionType::Start, false});
    if (cur.power && !m_prev.power) out.push_back({ActionType::Power, false});
    if (cur.volUp && !m_prev.volUp) out.push_back({ActionType::VolUp, false});
    if (cur.volDown && !m_prev.volDown)
      out.push_back({ActionType::VolDown, false});
  }

  SDL_Joystick* m_joy = nullptr;
  SDL_JoystickID m_joyIndex = -1;
  bool m_debug = false;
  State m_prev;
  Uint32 m_repeatStart = 0;
  Uint32 m_lastMenuAt = 0;
  Uint32 m_lastRepeat = 0;
  bool m_captureWaitRelease = true;
  CaptureState m_capturePrev;
  Sint16 m_axisPrev[32] = {0};  // last poll's axis values (push detection)
};

Input* Input::create() { return new InputImpl(); }

}  // namespace n64ui
