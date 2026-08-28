// GameMenu: the in-game pause menu plus its sub-menus (Save/Load slots,
// Reset, Speed, Video, Input, Save Settings, Cheats). Built on the modular
// MenuScreen base.
#pragma once

#include <string>

#include "screens/MenuScreen.h"

namespace n64ui {

class GameMenu : public MenuScreen {
 public:
  GameMenu(Emulator& emu, Input& input);
  ~GameMenu() override;
  void onShow() override;
  void onHide() override;
  std::string title() const override;
  std::string footer() const override;

 private:
  Input* m_input = nullptr;
};

class SlotMenu : public MenuScreen {
 public:
  SlotMenu(Emulator& emu, bool save);
  std::string title() const override;

 private:
  std::string slotName(int i) const;
  std::string slotInfo(int slot) const;
  bool m_save = true;
};

class ResetMenu : public MenuScreen {
 public:
  explicit ResetMenu(Emulator& emu);
  std::string title() const override;
};

class SpeedMenu : public MenuScreen {
 public:
  explicit SpeedMenu(Emulator& emu);
  std::string title() const override;
};

class VideoMenu : public MenuScreen {
 public:
  explicit VideoMenu(Emulator& emu);
  std::string title() const override;

 private:
  std::string wrapperResName() const;
};

class WrapperResMenu : public MenuScreen {
 public:
  explicit WrapperResMenu(Emulator& emu);
  std::string title() const override;

 private:
  void setRes(int v);
};

class AudioMenu : public MenuScreen {
 public:
  explicit AudioMenu(Emulator& emu);
  std::string title() const override;

 private:
  std::string resamplerName() const;
};

class SampleRateMenu : public MenuScreen {
 public:
  explicit SampleRateMenu(Emulator& emu);
  std::string title() const override;
};

class ResamplerMenu : public MenuScreen {
 public:
  explicit ResamplerMenu(Emulator& emu);
  std::string title() const override;
};

class SaveSettingsMenu : public MenuScreen {
 public:
  explicit SaveSettingsMenu(Emulator& emu);
  std::string title() const override;
};

}  // namespace n64ui
