// ROM list: scan the ROM directory, parse N64 headers (name, CRC1/2, country)
// without touching the core, handle zip/7z by temp-extraction at launch.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace n64ui {

struct RomEntry {
  std::string path;       // full path on disk (temp file if archived)
  std::string name;       // header name, trimmed
  std::string country;    // "U" "E" "J" etc.
  uint32_t crc1 = 0;
  uint32_t crc2 = 0;
  bool isArchive = false; // needs temp extraction before launch
};

class RomList {
 public:
  virtual ~RomList() = default;

  // Scan dir for *.n64/*.z64/*.v64/*.zip/*.7z, parse headers. Sorted by name.
  virtual bool scan(const std::string& dir) = 0;

  virtual const std::vector<RomEntry>& entries() const = 0;

  // Resolve entry i to a launchable path: archives are extracted to a temp
  // file (caller must delete the returned path if it differs from entry.path).
  virtual std::string launchablePath(size_t index) = 0;

  static RomList* create();
};

}  // namespace n64ui
