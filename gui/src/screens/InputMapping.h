// InputMappingMenu: map N64 controls to the user's actual input (keyboard on
// the host, the Brick's buttons on device). Writes Input-SDL-Control1 keys.
#pragma once

#include <string>
#include <vector>

#include "screens/MenuScreen.h"

namespace n64ui {

class InputMappingMenu : public MenuScreen {
 public:
  explicit InputMappingMenu(Emulator& emu);
  std::string title() const override;
  std::string footer() const override;

  ScreenResult handleAction(const Action& action) override;
  bool wantsRawCapture() override;
  void submitRawCapture(const std::string& binding, bool cancelled) override;
  void captureTick() override;
  void draw(Renderer& renderer) override;

 private:
  struct Control {
    std::string configKey;  // Input-SDL-Control1 key ("A Button", "X Axis"...)
    std::string label;      // display label
  };

  std::string bindingFor(const Control& c) const;
  void writeBinding(int index, const std::string& binding);

  std::vector<Control> m_controls;
  int m_capturing = -1;   // index being captured, -1 = idle
  Uint32 m_captureStart = 0;
};

}  // namespace n64ui
