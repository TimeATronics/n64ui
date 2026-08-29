// InputMappingMenu: map N64 controls to the user's actual input (keyboard on
// the host, the Brick's buttons on device). Writes Input-SDL-Control1 keys.
#pragma once

#include <string>
#include <vector>

#include "screens/MenuScreen.h"
#include "ui/Input.h"

namespace n64ui {

class InputMappingMenu : public MenuScreen {
 public:
  InputMappingMenu(Emulator& emu, Input& input);
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
    bool axisPair = false;  // analog entry: one capture fills the whole pair
  };

  std::string bindingFor(const Control& c) const;
  std::string writeBinding(int index, const std::string& binding);
  std::string unmapDuplicates(int index, const std::string& value);
  void resetAll();

  std::vector<Control> m_controls;
  int m_capturing = -1;   // index being captured, -1 = idle
  Uint32 m_captureStart = 0;
  Input* m_input = nullptr;
};

}  // namespace n64ui
