// RomListImpl: directory scan + N64 header parse (self-contained; the header
// layout is fixed: magic at 0, name at 0x20, CRC1/2 at 0x10, country at 0x3E).
#include "roms/RomList.h"

#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>
#include <cstdio>
#include <cstring>

#include "util/Log.h"
#include "util/Str.h"

namespace n64ui {

namespace {

constexpr uint32_t kMagicBE = 0x80371240;  // .z64
constexpr uint32_t kMagicLE = 0x37804012;  // .n64
constexpr uint32_t kMagicV64 = 0x12408037;  // .v64 (byteswapped)

bool readHeader(const std::string& path, RomEntry& e) {
  FILE* f = fopen(path.c_str(), "rb");
  if (!f) return false;
  unsigned char hdr[0x40] = {0};
  size_t got = fread(hdr, 1, sizeof(hdr), f);
  fclose(f);
  if (got < 0x40) return false;

  uint32_t magic;
  memcpy(&magic, hdr, 4);
  uint32_t crc1, crc2;
  memcpy(&crc1, hdr + 0x10, 4);
  memcpy(&crc2, hdr + 0x14, 4);

  if (magic != kMagicBE && magic != kMagicLE && magic != kMagicV64) return false;

  e.crc1 = __builtin_bswap32(crc1);
  e.crc2 = __builtin_bswap32(crc2);

  char name[21] = {0};
  memcpy(name, hdr + 0x20, 20);
  e.name = strTrim(std::string(name));

  char country = (char)hdr[0x3E];
  switch (country) {
    case 'E': case 'D': case 'F': case 'I': case 'J': case 'P': case 'S':
      e.country = std::string(1, country);
      break;
    default: e.country = "?"; break;
  }
  return true;
}

bool isArchive(const std::string& path) {
  return strEndsWith(strLower(path), ".zip") || strEndsWith(strLower(path), ".7z");
}

bool isRom(const std::string& path) {
  std::string l = strLower(path);
  return strEndsWith(l, ".n64") || strEndsWith(l, ".z64") ||
         strEndsWith(l, ".v64") || isArchive(path);
}

}  // namespace

class RomListImpl : public RomList {
 public:
  bool scan(const std::string& dir) override {
    m_entries.clear();
    DIR* d = opendir(dir.c_str());
    if (!d) {
      LOG_WARN("roms: cannot open %s", dir.c_str());
      return false;
    }
    dirent* e;
    while ((e = readdir(d)) != nullptr) {
      std::string full = dir + "/" + e->d_name;
      if (!isRom(e->d_name)) continue;
      struct stat st;
      if (stat(full.c_str(), &st) != 0 || !S_ISREG(st.st_mode)) continue;
      RomEntry entry;
      entry.path = full;
      entry.isArchive = isArchive(full);
      if (entry.isArchive) {
        entry.name = strBaseName(full);
        entry.name = entry.name.substr(0, entry.name.find_last_of('.'));
        entry.country = "?";
      } else if (!readHeader(full, entry)) {
        continue;
      }
      m_entries.push_back(entry);
    }
    closedir(d);
    std::sort(m_entries.begin(), m_entries.end(),
              [](const RomEntry& a, const RomEntry& b) { return a.name < b.name; });
    LOG_INFO("roms: %zu games in %s", m_entries.size(), dir.c_str());
    return true;
  }

  const std::vector<RomEntry>& entries() const override { return m_entries; }

  std::string launchablePath(size_t index) override {
    const RomEntry& e = m_entries.at(index);
    if (!e.isArchive) return e.path;
    // Phase 3: extract with `7zzs e <archive> -so > tmp` (as the stock script).
    LOG_WARN("roms: archive extraction not implemented yet");
    return e.path;
  }

 private:
  std::vector<RomEntry> m_entries;
};

RomList* RomList::create() { return new RomListImpl(); }

}  // namespace n64ui
