// CheatScreenImpl: skeleton; Phase 3 parses mupencheat.txt via roms/Cheats.h.
#include "screens/CheatScreen.h"

#include "core/Emulator.h"
#include "util/Log.h"

namespace n64ui {

CheatScreen::CheatScreen(Emulator& emu, const std::string& romName)
    : m_emu(emu), m_romName(romName) {}

CheatScreen::~CheatScreen() = default;

ScreenResult CheatScreen::handleAction(const Action& action) {
  switch (action.type) {
    case ActionType::Down:
      m_view.move(1);
      break;
    case ActionType::Up:
      m_view.move(-1);
      break;
    case ActionType::A:
      toggleSelected();
      break;
    case ActionType::B:
    case ActionType::Menu:
      return {ScreenResult::Pop, nullptr};
    default:
      break;
  }
  return {ScreenResult::None, nullptr};
}

void CheatScreen::draw(Renderer& renderer) {
  renderer.drawRect(0, 0, 1024, 768, Rgba::rgb(16, 16, 16));
  m_view.draw(renderer, 0, 24, 1024, 768 - 44, Rgba::rgb(220, 220, 220),
              Rgba::rgb(70, 70, 140), Rgba::rgb(255, 255, 255));
  drawScreenChrome(renderer, 1024, 768, "Cheats", "A: toggle  B: back");
}

void CheatScreen::refresh() {
  // Phase 3: m_cheats->load(m_romName); build m_lines.
  m_lines.clear();
  m_view.setRows(m_lines);
  LOG_INFO("cheats: refresh for %s (Phase 3)", m_romName.c_str());
}

void CheatScreen::toggleSelected() {
  LOG_INFO("cheats: toggle %d (Phase 3)", m_view.selection());
}

}  // namespace n64ui
