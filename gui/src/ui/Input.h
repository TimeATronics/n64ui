// Input: maps SDL joystick/keyboard events to semantic Actions using the
// Brick's verified button layout (TRIMUI Player1).
//   A=1, B=0, X=3, Y=2, L1=4, R1=5, Select=6, Start=7, Menu=8; dpad = hat.
#pragma once

#include <string>
#include <vector>

#include <SDL.h>

namespace n64ui {

enum class ActionType {
  None,
  Up,
  Down,
  Left,
  Right,
  A,
  B,
  X,
  Y,
  L1,
  R1,
  Select,
  Start,
  Menu,
  Power,
  VolUp,
  VolDown,
};

struct Action {
  ActionType type = ActionType::None;
  bool repeat = false;  // true for dpad auto-repeat ticks
};

class Input {
 public:
  virtual ~Input() = default;

  // SDL_Init joystick subsystem and open device 0. No video init here.
  virtual bool init() = 0;
  virtual void shutdown() = 0;

  // Block until input or timeoutMs elapses. Returns true if an event arrived.
  virtual bool waitEvent(int timeoutMs) = 0;

  // Drain queued events into actions. Auto-repeat for dpad (400ms/120ms).
  virtual std::vector<Action> pollActions() = 0;

  // Re-baseline edge detection (call after screen push/pop so a still-held
  // Enter/B from the previous screen doesn't consume the next press).
  virtual void resync() = 0;

  // Block (max timeoutMs) until all UI keys (A/B/dpad/Menu) are released.
  // Used before resuming the game so keys held in the menu don't reach the
  // emulated controller.
  virtual void waitForRelease(int timeoutMs) = 0;

  // Non-blocking raw-input capture poll: checks the joystick/keyboard STATE
  // (not the event queue -- the input-sdl plugin's SDL pump races us for
  // events) for a key press / joystick button / hat / axis and returns the
  // input-sdl binding string ("key(4)", "button(1)", "hat(0 Up)", "axis(0+)")
  // in out. Returns true when an input was captured. Escape cancels (returns
  // false with out empty). beginCapture() arms it: it first waits until all
  // inputs are released so the triggering press (the A key used to enter
  // capture) doesn't map itself.
  virtual void beginCapture() = 0;
  virtual bool capturePoll(std::string& out) = 0;

  static Input* create();
};

}  // namespace n64ui
