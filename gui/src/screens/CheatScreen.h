// CheatScreen: list cheats for the current ROM (from mupencheat.txt), toggle
// each on/off via the core.
#pragma once

#include <string>
#include <vector>

#include "ui/Input.h"
#include "ui/Renderer.h"
#include "ui/Screen.h"
#include "ui/Widgets.h"

namespace n64ui {

class Emulator;

class CheatScreen : public Screen {
 public:
  // romName: current ROM header name; used to match mupencheat.txt sections.
  CheatScreen(Emulator& emu, const std::string& romName);
  ~CheatScreen() override;

  ScreenResult handleAction(const Action& action) override;
  void draw(Renderer& renderer) override;

 private:
  void refresh();
  void toggleSelected();

  Emulator& m_emu;
  std::string m_romName;
  ListView m_view;
  std::vector<std::string> m_lines;  // "name  [ON/OFF]"
};

}  // namespace n64ui
