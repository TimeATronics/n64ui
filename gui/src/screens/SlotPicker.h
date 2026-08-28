// SlotPicker: savestate slot list (0-9) with existing-file timestamps.
// mode = save or load.
#pragma once

#include <string>
#include <vector>

#include "ui/Input.h"
#include "ui/Renderer.h"
#include "ui/Screen.h"
#include "ui/Widgets.h"

namespace n64ui {

class Emulator;

class SlotPicker : public Screen {
 public:
  enum Mode { Save, Load };
  SlotPicker(Emulator& emu, const std::string& saveDir, Mode mode);
  ~SlotPicker() override;

  ScreenResult handleAction(const Action& action) override;
  void draw(Renderer& renderer) override;

 private:
  void refresh();
  void pick();

  Emulator& m_emu;
  std::string m_saveDir;
  Mode m_mode = Save;
  ListView m_view;
  std::vector<std::string> m_lines;  // "slot N  [timestamp]" or "(empty)"
};

}  // namespace n64ui
