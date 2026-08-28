// MenuScreen: plug-and-play menu base. Subclass and add() entries in the
// constructor. Each entry has a label, an optional value getter (settings
// rows) and an onActivate callback that runs on A: it may run a command,
// return Pop to close, or return Push(screen) to open a sub-menu.
#pragma once

#include <functional>
#include <string>
#include <vector>

#include "core/ConfigApi.h"
#include "core/Emulator.h"
#include "ui/Input.h"
#include "ui/Screen.h"
#include "ui/Widgets.h"

namespace n64ui {

inline ScreenResult screenPop() { return {ScreenResult::Pop, nullptr}; }
inline ScreenResult screenPush(Screen* s) { return {ScreenResult::Push, s}; }

class MenuScreen : public Screen {
 public:
  struct Entry {
    std::string label;
    // Right-aligned value text (e.g. "Speed: 150%"); refreshed each frame.
    std::function<std::string()> value = nullptr;
    // Runs on A. Return None/Pop/Push.
    std::function<ScreenResult()> onActivate = nullptr;
    // Optional: adjust the value with Left/Right (slider-style rows).
    std::function<void()> onLeft = nullptr;
    std::function<void()> onRight = nullptr;
  };

  explicit MenuScreen(Emulator& emu) : m_emu(emu) {}

  void add(std::string label, std::function<ScreenResult()> onActivate,
           std::function<std::string()> value = nullptr) {
    m_entries.push_back(
        {std::move(label), std::move(value), std::move(onActivate), nullptr,
         nullptr});
  }

  void addAdjust(std::string label, std::function<std::string()> value,
                 std::function<void()> onLeft, std::function<void()> onRight) {
    m_entries.push_back({std::move(label), std::move(value), nullptr,
                         std::move(onLeft), std::move(onRight)});
  }

  void clear() { m_entries.clear(); }

  // Show a transient bottom-of-screen message (drawn by any MenuScreen).
  // Static so it survives sub-menu push/pop (screens are deleted on pop).
  static void showToast(std::string msg) {
    s_toast = std::move(msg);
    s_toastUntil = SDL_GetTicks() + 2000;
  }

  // Standard two save entries for settings sub-menus.
  void addSaveEntries() {
    add("Save for this Game", [this] {
      bool ok = emu().saveSettingsPerGame();
      showToast(ok ? "Settings saved for this game" : "No ROM open");
      return ScreenResult{ScreenResult::None, nullptr};
    });
    add("Save Globally", [this] {
      emu().config().saveFile();
      showToast("Settings saved globally");
      return ScreenResult{ScreenResult::None, nullptr};
    });
  }

  ScreenResult handleAction(const Action& action) override {
    switch (action.type) {
      case ActionType::Down:
        m_list.move(1);
        break;
      case ActionType::Up:
        m_list.move(-1);
        break;
      case ActionType::Left: {
        if (!m_entries.empty()) {
          Entry& e = m_entries[m_list.selection()];
          if (e.onLeft) e.onLeft();
        }
        break;
      }
      case ActionType::Right: {
        if (!m_entries.empty()) {
          Entry& e = m_entries[m_list.selection()];
          if (e.onRight) e.onRight();
        }
        break;
      }
      case ActionType::A: {
        if (m_entries.empty()) break;
        Entry& e = m_entries[m_list.selection()];
        if (e.onActivate) return e.onActivate();
        break;
      }
      case ActionType::B:
      case ActionType::Menu:
        return screenPop();
      default:
        break;
    }
    return {ScreenResult::None, nullptr};
  }

  void draw(Renderer& renderer) override {
    // Refresh right-side values (settings rows) before drawing.
    std::vector<std::string> labels, values;
    labels.reserve(m_entries.size());
    values.reserve(m_entries.size());
    for (auto& e : m_entries) {
      labels.push_back(e.label);
      values.push_back(e.value ? e.value() : "");
    }
    m_list.setRows(labels, values);

    renderer.setFontSize(fontPx());
    renderer.drawRect(0, 0, kScreenW, kScreenH, Rgba::rgba(16, 16, 24, 255));
    renderer.drawRectOutline(0, 0, kScreenW, kScreenH, Rgba::rgb(90, 90, 90));
    drawScreenChrome(renderer, kScreenW, kScreenH, title(), footer());
    m_list.draw(renderer, 32, 110, kScreenW - 64, kScreenH - 110 - 64,
                Rgba::rgb(230, 230, 230), Rgba::rgb(70, 70, 78),
                Rgba::rgb(255, 255, 255));
    // Transient toast at the bottom.
    if (!s_toast.empty() && s_toastUntil > SDL_GetTicks()) {
      int prevSize = fontPx();
      renderer.setFontSize(24);
      int tw = renderer.textWidth(s_toast);
      int bw = tw + 48;
      int bx = (kScreenW - bw) / 2;
      int by = kScreenH - 72;
      renderer.drawRect(bx, by, bw, 44, Rgba::rgb(60, 60, 72));
      renderer.drawRectOutline(bx, by, bw, 44, Rgba::rgb(200, 200, 200));
      renderer.drawText(bx + 24, by + 10, s_toast, Rgba::rgb(255, 255, 255));
      renderer.setFontSize(prevSize);
    }
  }

 protected:
  Emulator& emu() const { return m_emu; }
  const std::vector<Entry>& entries() const { return m_entries; }

  virtual int fontPx() const { return 40; }
  virtual std::string title() const { return "N64"; }
  virtual std::string footer() const { return "A: select  B: back"; }

  static constexpr int kScreenW = 1024;
  static constexpr int kScreenH = 768;

 private:
  Emulator& m_emu;
  ListView m_list;
  std::vector<Entry> m_entries;
  inline static std::string s_toast;
  inline static Uint32 s_toastUntil = 0;
};

}  // namespace n64ui
