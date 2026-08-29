// InputMappingMenu: map N64 controls to the user's actual input (keyboard on
// the host, the Brick's buttons on device). Writes Input-SDL-Control1 keys.
#include "screens/InputMapping.h"

#include <cstring>

#include <SDL.h>

#include "core/ConfigApi.h"
#include "util/Log.h"
#include "util/Platform.h"
#include "util/Str.h"

namespace n64ui {

namespace {
constexpr const char* kSection = "Input-SDL-Control1";

// Default bindings for "Reset All" (device): verified Brick button layout
// (A=1, B=0, Start=7), dpad = hat, and the trigger axes L2=axis 2, R2=axis
// 5 (they rest at -32768 and press to +32767; the stock's 24000 deadzone
// style keeps the rest state from firing).
const char* kDefaultsDevice[][2] = {
    {"A Button", "button(1)"},
    {"B Button", "button(0)"},
    {"Start", "button(7)"},
    {"Z Trig", "axis(2+,24000)"},
    {"DPad U", "hat(0 Up)"},
    {"DPad D", "hat(0 Down)"},
    {"DPad L", "hat(0 Left)"},
    {"DPad R", "hat(0 Right)"},
    {"C Button U", ""},
    {"C Button D", ""},
    {"C Button L", ""},
    {"C Button R", "axis(5+,24000)"},
    {"L Trig", "button(4)"},
    {"R Trig", "button(5)"},
    {"X Axis", "axis(0-,0+)"},
    {"Y Axis", "axis(1-,1+)"},
    {"Mempak switch", ""},
    {"Rumblepak switch", ""},
};

// Host keyboard defaults (same layout as the UI's own keyboard controls).
const char* kDefaultsHost[][2] = {
    {"A Button", "key(40)"},     // Enter
    {"B Button", "key(42)"},     // Backspace
    {"Start", "key(44)"},        // Space
    {"Z Trig", "key(20)"},       // Q
    {"DPad U", "key(82)"},       // Up
    {"DPad D", "key(81)"},       // Down
    {"DPad L", "key(80)"},       // Left
    {"DPad R", "key(79)"},       // Right
    {"C Button U", "key(22)"},   // U
    {"C Button D", "key(7)"},    // D
    {"C Button L", "key(15)"},   // J
    {"C Button R", "key(16)"},   // K
    {"L Trig", "key(20)"},       // Q
    {"R Trig", "key(8)"},        // E
    {"X Axis", "key(80,79)"},
    {"Y Axis", "key(82,81)"},
    {"Mempak switch", ""},
    {"Rumblepak switch", ""},
};
}  // namespace

InputMappingMenu::InputMappingMenu(Emulator& emu, Input& input)
    : MenuScreen(emu), m_input(&input) {
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
      {"X Axis", "Analog X (left/right)", true},
      {"Y Axis", "Analog Y (up/down)", true},
      {"Mempak switch", "Mem Pak Switch"},
      {"Rumblepak switch", "Rumble Pak Switch"},
  };
  for (size_t i = 0; i < m_controls.size(); ++i) {
    const Control& c = m_controls[i];
    int idx = (int)i;
    add(c.label, [this, idx] {
      m_capturing = idx;
      m_captureStart = SDL_GetTicks();
      m_input->beginCapture();
      return ScreenResult{ScreenResult::None, nullptr};
    }, [this, idx] { return bindingFor(m_controls[idx]); });
  }
  add("Reset All Controls", [this] {
    resetAll();
    return ScreenResult{ScreenResult::None, nullptr};
  });
  addSaveEntries();
}

std::string InputMappingMenu::title() const { return "Input Mapping"; }
std::string InputMappingMenu::footer() const {
  return m_capturing >= 0 ? "Press a key/button..." : "A: remap  B: back";
}

ScreenResult InputMappingMenu::handleAction(const Action& action) {
  if (m_capturing >= 0) {
    // While capturing, ignore menu navigation (raw events go to capture).
    // B is NOT the cancel here so B itself can be mapped; only Menu (F2 on
    // the Brick, Escape on the host) cancels.
    if (action.type == ActionType::Menu) {
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
  std::string stored = writeBinding(m_capturing, binding);
  std::string cleared = unmapDuplicates(m_capturing, stored);
  std::string msg = strFormat("%s: %s", m_controls[m_capturing].label.c_str(),
                              binding.c_str());
  if (!cleared.empty())
    msg += strFormat(" | %s cleared (same input)", cleared.c_str());
  showToast(msg);
  LOG_INFO("input map: %s = %s", m_controls[m_capturing].configKey.c_str(),
           binding.c_str());
  if (!cleared.empty())
    LOG_INFO("input map: cleared duplicate on %s", cleared.c_str());
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

std::string InputMappingMenu::writeBinding(int index,
                                           const std::string& binding) {
  m64p_handle h = nullptr;
  if (emu().config().openSection(kSection, &h) != M64ERR_SUCCESS) return "";
  const Control& c = m_controls[index];
  std::string value = binding;
  if (c.axisPair) {
    // Analog entries store the full negative/positive pair in one key. If
    // the capture was an axis binding, mirror it (left/right or up/down) so
    // a single dpad press sets both directions - e.g. capturing "axis(0-)"
    // stores "axis(0-,0+)". Non-axis captures are stored as-is.
    int n = 0;
    char sign = 0;
    if (sscanf(binding.c_str(), "axis(%d%c)", &n, &sign) == 2) {
      value = strFormat("axis(%d-,%d+)", n, n);
    }
  }
  emu().config().setString(h, c.configKey.c_str(), value.c_str());
  emu().config().saveFile();
  return value;
}

// If the just-stored binding is already used by another control, clear that
// control (one input = one control). Returns the labels that were cleared.
std::string InputMappingMenu::unmapDuplicates(int index,
                                              const std::string& value) {
  if (value.empty()) return "";
  m64p_handle h = nullptr;
  if (emu().config().openSection(kSection, &h) != M64ERR_SUCCESS) return "";
  std::string cleared;
  for (size_t i = 0; i < m_controls.size(); ++i) {
    if ((int)i == index) continue;
    char buf[256] = {0};
    int len = sizeof(buf);
    if (emu().config().getString(h, m_controls[i].configKey.c_str(), buf,
                                 &len) == M64ERR_SUCCESS &&
        value == buf) {
      emu().config().setString(h, m_controls[i].configKey.c_str(), "");
      if (!cleared.empty()) cleared += ", ";
      cleared += m_controls[i].label;
    }
  }
  if (!cleared.empty()) emu().config().saveFile();
  return cleared;
}

void InputMappingMenu::resetAll() {
  // Back up the current bindings first (the user can always restore them
  // from this file), then write the platform defaults.
  m64p_handle h = nullptr;
  if (emu().config().openSection(kSection, &h) == M64ERR_SUCCESS) {
    std::string bak;
    for (const Control& c : m_controls) {
      char buf[256] = {0};
      int len = sizeof(buf);
      if (emu().config().getString(h, c.configKey.c_str(), buf, &len) ==
          M64ERR_SUCCESS)
        bak += c.configKey + " = " + buf + "\n";
    }
    if (!bak.empty()) {
      std::string path =
          strFormat("%s/n64ui-Input1-backup.cfg", Platform::configDir().c_str());
      FILE* f = fopen(path.c_str(), "w");
      if (f) {
        fwrite(bak.data(), 1, bak.size(), f);
        fclose(f);
        LOG_INFO("input map: backed up to %s", path.c_str());
      }
    }
  }
  const char* (*defs)[2] =
      Platform::isDevice() ? kDefaultsDevice : kDefaultsHost;
  for (size_t i = 0; i < m_controls.size(); ++i) {
    writeBinding((int)i, defs[i][1]);
  }
  showToast("All controls reset to default");
}

}  // namespace n64ui
