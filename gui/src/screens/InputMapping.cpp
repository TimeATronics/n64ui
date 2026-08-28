// InputMappingMenu: map N64 controls to the user's actual input (keyboard on
// the host, the Brick's buttons on device). Writes Input-SDL-Control1 keys.
#include "screens/InputMapping.h"

#include <cstring>

#include <SDL.h>

#include "core/ConfigApi.h"
#include "util/Log.h"
#include "util/Str.h"

namespace n64ui {

namespace {
constexpr const char* kSection = "Input-SDL-Control1";
}  // namespace

InputMappingMenu::InputMappingMenu(Emulator& emu) : MenuScreen(emu) {
  m_controls = {
      {"A Button", "A Button"},
      {"B Button", "B Button"},
      {"Start", "Start"},
      {"Z Trig", "Z Trigger"},
      {"DPad U", "D-Pad Up"},
      {"DPad D", "D-Pad Down"},
      {"DPad L", "D-Pad Left"},
      {"DPad R", "D-Pad Right"},
      {"C Button U", "C Up"},
      {"C Button D", "C Down"},
      {"C Button L", "C Left"},
      {"C Button R", "C Right"},
      {"L Trig", "L Trigger"},
      {"R Trig", "R Trigger"},
      {"X Axis", "Analog X (left/right)"},
      {"Y Axis", "Analog Y (up/down)"},
      {"Mempak switch", "Mem Pak Switch"},
      {"Rumblepak switch", "Rumble Pak Switch"},
  };
  for (size_t i = 0; i < m_controls.size(); ++i) {
    const Control& c = m_controls[i];
    int idx = (int)i;
    add(c.label, [this, idx] {
      m_capturing = idx;
      m_captureStart = SDL_GetTicks();
      return ScreenResult{ScreenResult::None, nullptr};
    }, [this, idx] { return bindingFor(m_controls[idx]); });
  }
  addSaveEntries();
}

std::string InputMappingMenu::title() const { return "Input Mapping"; }
std::string InputMappingMenu::footer() const {
  return m_capturing >= 0 ? "Press a key/button...  Esc: cancel"
                          : "A: remap  B: back";
}

ScreenResult InputMappingMenu::handleAction(const Action& action) {
  if (m_capturing >= 0) {
    // While capturing, ignore menu navigation (raw events go to capture).
    if (action.type == ActionType::B || action.type == ActionType::Menu) {
      m_capturing = -1;
      return {ScreenResult::None, nullptr};
    }
    return {ScreenResult::None, nullptr};
  }
  return MenuScreen::handleAction(action);
}

bool InputMappingMenu::wantsRawCapture() { return m_capturing >= 0; }

void InputMappingMenu::captureTick() {
  // Auto-cancel after 15s so the UI never waits forever.
  if (m_capturing >= 0 && SDL_GetTicks() - m_captureStart > 15000) {
    showToast("Capture cancelled");
    m_capturing = -1;
  }
}

void InputMappingMenu::submitRawCapture(const std::string& binding,
                                        bool cancelled) {
  if (cancelled || m_capturing < 0) {
    if (cancelled) showToast("Capture cancelled");
    m_capturing = -1;
    return;
  }
  writeBinding(m_capturing, binding);
  showToast(strFormat("%s: %s", m_controls[m_capturing].label.c_str(),
                      binding.c_str()));
  LOG_INFO("input map: %s = %s", m_controls[m_capturing].configKey.c_str(),
           binding.c_str());
  m_capturing = -1;
}

void InputMappingMenu::draw(Renderer& renderer) {
  MenuScreen::draw(renderer);
  if (m_capturing >= 0) {
    // Overlay hint that a capture is in progress.
    renderer.setFontSize(24);
    const char* msg = "Press a key or button for...";
    int w = renderer.textWidth(msg) + 48;
    int x = (kScreenW - w) / 2;
    renderer.drawRect(x, 40, w, 44, Rgba::rgb(60, 60, 72));
    renderer.drawRectOutline(x, 40, w, 44, Rgba::rgb(200, 200, 200));
    renderer.drawText(x + 24, 50, msg, Rgba::rgb(255, 255, 255));
    renderer.setFontSize(fontPx());
  }
}

std::string InputMappingMenu::bindingFor(const Control& c) const {
  char buf[256] = {0};
  int len = sizeof(buf);
  m64p_handle h = nullptr;
  if (emu().config().openSection(kSection, &h) == M64ERR_SUCCESS &&
      emu().config().getString(h, c.configKey.c_str(), buf, &len) ==
          M64ERR_SUCCESS) {
    // Show a friendlier label for key(<scancode>).
    std::string b = buf;
    if (b.rfind("key(", 0) == 0) {
      int code = atoi(b.c_str() + 4);
      const char* name = SDL_GetScancodeName((SDL_Scancode)code);
      if (name && *name) return name;
    }
    return b;
  }
  return "(unset)";
}

void InputMappingMenu::writeBinding(int index, const std::string& binding) {
  m64p_handle h = nullptr;
  if (emu().config().openSection(kSection, &h) == M64ERR_SUCCESS) {
    emu().config().setString(h, m_controls[index].configKey.c_str(),
                             binding.c_str());
    emu().config().saveFile();
  }
}

}  // namespace n64ui
