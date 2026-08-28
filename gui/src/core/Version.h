// mupen64plus API version constants, pinned to our core build (2.6.0).
#pragma once

#include <cstdint>

namespace n64ui {

// What we pass to CoreStartup (see mupen64plus-core/src/api + ui-console 2.6.0).
constexpr uint32_t kCoreApiVersion = 0x020001;
constexpr uint32_t kConfigApiVersion = 0x020301;

// Our own frontend version (shown in --version).
constexpr uint32_t kFrontendVersion = 0x010000;

}  // namespace n64ui
