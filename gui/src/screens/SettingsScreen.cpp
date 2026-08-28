// SettingsScreenImpl: skeleton; Phase 3 wires the Setting table + editors.
#include "screens/SettingsScreen.h"

#include "config/SettingsStore.h"
#include "util/Log.h"

namespace n64ui {

SettingsScreen::SettingsScreen(SettingsStore& store, const std::string& filter)
    : m_store(store), m_filter(filter) {}

SettingsScreen::~SettingsScreen() = default;

void SettingsScreen::onHide() {
  if (m_dirty) m_store.save();
  m_dirty = false;
}

ScreenResult SettingsScreen::handleAction(const Action& action) {
  switch (action.type) {
    case ActionType::Down:
      m_view.move(1);
      break;
    case ActionType::Up:
      m_view.move(-1);
      break;
    case ActionType::A:
      editSelected();
      break;
    case ActionType::B:
    case ActionType::Menu:
      return {ScreenResult::Pop, nullptr};
    default:
      break;
  }
  return {ScreenResult::None, nullptr};
}

void SettingsScreen::draw(Renderer& renderer) {
  renderer.drawRect(0, 0, 1024, 768, Rgba::rgb(16, 16, 16));
  m_view.draw(renderer, 0, 24, 1024, 768 - 44, Rgba::rgb(220, 220, 220),
              Rgba::rgb(70, 70, 140), Rgba::rgb(255, 255, 255));
  drawScreenChrome(renderer, 1024, 768, m_filter.empty() ? "Settings" : m_filter,
                   "A: edit  B: back");
}

void SettingsScreen::rebuildRows() {
  // Phase 3: m_store.settings(m_filter) -> m_rows; build value strings.
  m_rows.clear();
  m_view.setRows({});
  LOG_INFO("settings: rebuild rows (Phase 3)");
}

void SettingsScreen::editSelected() {
  // Phase 3: switch on Setting.kind -> editor popup screen.
  LOG_INFO("settings: edit row %d (Phase 3)", m_view.selection());
}

std::string SettingsScreen::valueString(const Setting& s) const {
  // Phase 3: read current value from m_store.
  (void)s;
  return "";
}

}  // namespace n64ui
