// SlotPickerImpl: skeleton; Phase 2 lists save/<name>.st<N> with mtimes.
#include "screens/SlotPicker.h"

#include "core/Emulator.h"
#include "util/Log.h"

namespace n64ui {

SlotPicker::SlotPicker(Emulator& emu, const std::string& saveDir, Mode mode)
    : m_emu(emu), m_saveDir(saveDir), m_mode(mode) {}

SlotPicker::~SlotPicker() = default;

ScreenResult SlotPicker::handleAction(const Action& action) {
  switch (action.type) {
    case ActionType::Down:
      m_view.move(1);
      break;
    case ActionType::Up:
      m_view.move(-1);
      break;
    case ActionType::A:
      pick();
      break;
    case ActionType::B:
    case ActionType::Menu:
      return {ScreenResult::Pop, nullptr};
    default:
      break;
  }
  return {ScreenResult::None, nullptr};
}

void SlotPicker::draw(Renderer& renderer) {
  renderer.drawRect(0, 0, 1024, 768, Rgba::rgb(16, 16, 16));
  m_view.draw(renderer, 0, 24, 1024, 768 - 44, Rgba::rgb(220, 220, 220),
              Rgba::rgb(70, 70, 140), Rgba::rgb(255, 255, 255));
  drawScreenChrome(renderer, 1024, 768,
                   m_mode == Save ? "Save State" : "Load State",
                   "A: confirm  B: back");
}

void SlotPicker::refresh() {
  // Phase 2: glob m_saveDir for *.st[0-9], stat mtime, build m_lines.
  m_lines.clear();
  m_view.setRows(m_lines);
  LOG_INFO("slotpicker: refresh %s (Phase 2)", m_saveDir.c_str());
}

void SlotPicker::pick() {
  if (m_mode == Save) m_emu.saveState(m_view.selection());
  else m_emu.loadState(m_view.selection());
}

}  // namespace n64ui
