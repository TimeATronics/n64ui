#include "util/Log.h"

#include <cstdarg>
#include <cstdio>

namespace n64ui {

static LogLevel g_level = LogLevel::Info;

void logSetLevel(LogLevel level) { g_level = level; }
LogLevel logLevel() { return g_level; }

void logPrint(LogLevel level, const char* fmt, ...) {
  if (static_cast<int>(level) > static_cast<int>(g_level)) return;
  static const char* kPrefix[] = {"[ERR]", "[WARN]", "[INFO]", "[DBG]"};
  char buf[512];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(buf, sizeof(buf), fmt, ap);
  va_end(ap);
  fprintf(stderr, "%s %s\n", kPrefix[static_cast<int>(level)], buf);
}

}  // namespace n64ui
