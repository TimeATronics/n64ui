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

namespace n64ui {

namespace {
constexpr int kRepeatDelayMs = 400;
constexpr int kRepeatRateMs = 120;
}  // namespace

class InputImpl : public Input {
 public:
  bool init() override {
    SDL_Init(SDL_INIT_JOYSTICK | SDL_INIT_GAMECONTROLLER);
    if (Platform::isDevice()) {
      m_joy = SDL_JoystickOpen(0);
      if (m_joy) {
        LOG_INFO("joystick: %s (%d buttons)",
                 SDL_JoystickName(m_joy), SDL_JoystickNumButtons(m_joy));
      } else {
        LOG_WARN("no joystick 0: %s", SDL_GetError());
      }
    }
    m_prev = readState();
    return true;
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
    // Non-blocking: poll the queue once. The emu thread pumps SDL from its
    // own loop, so we must not block in WaitEventTimeout here (Xlib access
    // isn't multi-thread safe).
    SDL_PumpEvents();
    SDL_Event ev;
    while (SDL_PollEvent(&ev)) {
      switch (ev.type) {
        case SDL_KEYDOWN: {
          if (ev.key.keysym.scancode == SDL_SCANCODE_ESCAPE ||
              ev.key.keysym.scancode == SDL_SCANCODE_BACKSPACE)
            return false;  // cancel
          out = "key(" + std::to_string((int)ev.key.keysym.scancode) + ")";
          return true;
        }
        case SDL_JOYBUTTONDOWN:
          out = "button(" + std::to_string((int)ev.jbutton.button) + ")";
          return true;
        case SDL_JOYHATMOTION: {
          if (ev.jhat.value == SDL_HAT_CENTERED) break;
          const char* dir = (ev.jhat.value & SDL_HAT_UP)    ? "Up"
                            : (ev.jhat.value & SDL_HAT_DOWN)  ? "Down"
                            : (ev.jhat.value & SDL_HAT_LEFT)  ? "Left"
                                                              : "Right";
          out = "hat(" + std::to_string((int)ev.jhat.hat) + " " + dir + ")";
          return true;
        }
        case SDL_JOYAXISMOTION: {
          if (ev.jaxis.value > 16384 || ev.jaxis.value < -16384) {
            out = "axis(" + std::to_string((int)ev.jaxis.axis) +
                  (ev.jaxis.value > 0 ? "+" : "-") + ")";
            return true;
          }
          break;
        }
        default:
          break;
      }
    }
    return true;  // no input yet; keep waiting (out untouched)
  }

  // Sleep only; all input is read via pollActions (state polling).
  bool waitEvent(int timeoutMs) override {
    SDL_Event ev;
    return SDL_WaitEventTimeout(&ev, timeoutMs) == 1;
  }

  std::vector<Action> pollActions() override {
    std::vector<Action> out;
    State cur = readState();

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
  State m_prev;
  Uint32 m_repeatStart = 0;
  Uint32 m_lastMenuAt = 0;
  Uint32 m_lastRepeat = 0;
};

Input* Input::create() { return new InputImpl(); }

}  // namespace n64ui
