// Launcher screen: ROM browser. Lists games from the ROM path, launches on A.
#pragma once

#include <string>
#include <vector>

#include "ui/Input.h"
#include "ui/Renderer.h"
#include "ui/Screen.h"
#include "ui/Widgets.h"

namespace n64ui {

class Emulator;
class RomList;

class Launcher : public Screen {
 public:
  explicit Launcher(Emulator& emu, const std::string& romDir);
  ~Launcher() override;

  void onShow() override;
  ScreenResult handleAction(const Action& action) override;
  void draw(Renderer& renderer) override;

 private:
  void rescan();
  void launchSelected();

  Emulator& m_emu;
  std::string m_romDir;
  RomList* m_list = nullptr;
  ListView m_view;
  std::vector<std::string> m_lines;  // display strings, parallel to m_list
};

}  // namespace n64ui
