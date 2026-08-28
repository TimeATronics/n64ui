// LauncherImpl: skeleton. Phase 3 fills in RomList scanning + launch flow.
#include "screens/Launcher.h"

#include "core/Emulator.h"
#include "roms/RomList.h"
#include "util/Log.h"

namespace n64ui {

Launcher::Launcher(Emulator& emu, const std::string& romDir)
    : m_emu(emu), m_romDir(romDir) {
  m_list = RomList::create();
}

Launcher::~Launcher() { delete m_list; }

void Launcher::onShow() { rescan(); }

ScreenResult Launcher::handleAction(const Action& action) {
  switch (action.type) {
    case ActionType::Down:
      m_view.move(1);
      break;
    case ActionType::Up:
      m_view.move(-1);
      break;
    case ActionType::A:
      launchSelected();
      break;
    case ActionType::Menu:
      // Phase 3: push SettingsScreen.
      break;
    case ActionType::B:
      return {ScreenResult::Pop, nullptr};  // exit app -> launcher
    default:
      break;
  }
  return {ScreenResult::None, nullptr};
}

void Launcher::draw(Renderer& renderer) {
  renderer.setFontSize(24);
  renderer.drawRect(0, 0, 1024, 768, Rgba::rgb(16, 16, 16));
  m_view.draw(renderer, 0, 40, 1024, 768 - 80, Rgba::rgb(220, 220, 220),
              Rgba::rgb(70, 70, 140), Rgba::rgb(255, 255, 255));
  drawScreenChrome(renderer, 1024, 768, "N64", "A: play  B: exit  MENU: settings");
}

void Launcher::rescan() {
  // Phase 3: m_list->scan(m_romDir), build m_lines from names + region.
  m_lines.clear();
  m_view.setRows(m_lines);
  LOG_INFO("launcher: scanning %s", m_romDir.c_str());
}

void Launcher::launchSelected() {
  // Phase 3: m_list->path(m_view.selection()) -> m_emu.launch(path).
  LOG_INFO("launcher: launch not implemented yet");
}

}  // namespace n64ui
