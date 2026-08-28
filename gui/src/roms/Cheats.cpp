// CheatsImpl: mupencheat.txt parser. Skeleton: Phase 3 fills the format
// parsing ([CRC1-CRC2-C:X] sections, cheat name + code lines).
#include "roms/Cheats.h"

#include "core/Emulator.h"
#include "util/Log.h"

namespace n64ui {

class CheatsImpl : public Cheats {
 public:
  bool load(const std::string& dataDir, uint32_t crc1, uint32_t crc2,
            const std::string& country) override {
    (void)dataDir;
    (void)crc1;
    (void)crc2;
    (void)country;
    m_all.clear();
    // Phase 3: read <dataDir>/mupencheat.txt, find section
    // "[XXXXXXXX-XXXXXXXX-C:X]", parse name lines + code pairs.
    LOG_INFO("cheats: load for %08X-%08X (Phase 3)", (unsigned)crc1, (unsigned)crc2);
    return true;
  }

  const std::vector<Cheat>& all() const override { return m_all; }

  void apply(Emulator& emu) override {
    for (auto& c : m_all) {
      if (!c.codes.empty() && !c.enabled) continue;
      emu.addCheat(c.name, c.codes.data(), (int)c.codes.size());
      emu.setCheatEnabled(c.name, c.enabled);
    }
  }

  void toggle(size_t index, Emulator& emu) override {
    if (index >= m_all.size()) return;
    m_all[index].enabled = !m_all[index].enabled;
    emu.setCheatEnabled(m_all[index].name, m_all[index].enabled);
  }

 private:
  std::vector<Cheat> m_all;
};

Cheats* Cheats::create() { return new CheatsImpl(); }

}  // namespace n64ui
