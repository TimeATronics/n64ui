// SettingsScreen: data-driven settings list built from the Setting table.
// Each row is one Setting; A opens an editor (toggle / slider / choice),
// B saves and pops.
#pragma once

#include <string>
#include <vector>

#include "config/Settings.h"
#include "ui/Input.h"
#include "ui/Renderer.h"
#include "ui/Screen.h"
#include "ui/Widgets.h"

namespace n64ui {

class SettingsStore;

class SettingsScreen : public Screen {
 public:
  // sectionFilter: only show settings whose section matches (or all if empty).
  SettingsScreen(SettingsStore& store, const std::string& sectionFilter);
  ~SettingsScreen() override;

  void onHide() override;  // ConfigSaveFile

  ScreenResult handleAction(const Action& action) override;
  void draw(Renderer& renderer) override;

 private:
  void rebuildRows();
  void editSelected();
  std::string valueString(const Setting& s) const;

  SettingsStore& m_store;
  std::string m_filter;
  std::vector<const Setting*> m_rows;
  ListView m_view;
  bool m_dirty = false;
};

}  // namespace n64ui
