// Screen: abstract UI screen. Screens return a ScreenResult from
// handleAction; the main loop pushes/pops screens accordingly.
#pragma once

#include <string>

#include "ui/Input.h"
#include "ui/Renderer.h"

namespace n64ui {

class Screen;

struct ScreenResult {
  enum Kind { None, Pop, Push } kind = None;
  Screen* next = nullptr;  // for Push
};

class Screen {
 public:
  virtual ~Screen() = default;

  virtual void onShow() {}
  virtual void onHide() {}

  virtual ScreenResult handleAction(const Action& action) = 0;
  virtual void draw(Renderer& renderer) = 0;

  // Raw-input capture hooks (input mapping): the main loop polls the SDL
  // event queue each iteration while wantsRawCapture() is true and delivers
  // the result via submitRawCapture(). captureTick() is called every loop
  // iteration so the screen can auto-cancel after a timeout.
  virtual bool wantsRawCapture() { return false; }
  virtual void submitRawCapture(const std::string& binding, bool cancelled) {
    (void)binding;
    (void)cancelled;
  }
  virtual void captureTick() {}
};

}  // namespace n64ui
