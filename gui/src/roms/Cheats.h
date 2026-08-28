// Cheat database: parse mupencheat.txt for a ROM (section matched by
// CRC1-CRC2-C:country) and apply via the core's CoreAddCheat/CheatEnabled.
#pragma once

#include <string>
#include <vector>

#include "m64p_common.h"

namespace n64ui {

class Emulator;

struct Cheat {
  std::string name;
  std::vector<m64p_cheat_code> codes;
  bool enabled = false;
};

class Cheats {
 public:
  virtual ~Cheats() = default;

  // Load the cheat section for a ROM from mupencheat.txt (dataDir).
  virtual bool load(const std::string& dataDir, uint32_t crc1, uint32_t crc2,
                    const std::string& country) = 0;

  virtual const std::vector<Cheat>& all() const = 0;

  // Apply state to the core (add + enable all enabled cheats).
  virtual void apply(Emulator& emu) = 0;
  virtual void toggle(size_t index, Emulator& emu) = 0;

  static Cheats* create();
};

}  // namespace n64ui
