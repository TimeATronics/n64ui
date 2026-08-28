// Tiny MD5 (RFC 1321) for ROM fingerprinting (per-game config section key,
// savestate filenames). Self-contained, no external deps.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace n64ui {

// Returns the 32-char lowercase hex MD5 of the input bytes.
std::string md5Hex(const void* data, size_t len);
inline std::string md5Hex(const std::vector<unsigned char>& v) {
  return md5Hex(v.data(), v.size());
}

}  // namespace n64ui
