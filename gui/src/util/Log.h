// Minimal leveled logger. Stderr only, no allocation beyond format.
#pragma once

namespace n64ui {

enum class LogLevel { Error = 0, Warn, Info, Debug };

void logSetLevel(LogLevel level);
LogLevel logLevel();
void logPrint(LogLevel level, const char* fmt, ...);

#define LOG_ERROR(...) ::n64ui::logPrint(::n64ui::LogLevel::Error, __VA_ARGS__)
#define LOG_WARN(...)  ::n64ui::logPrint(::n64ui::LogLevel::Warn, __VA_ARGS__)
#define LOG_INFO(...)  ::n64ui::logPrint(::n64ui::LogLevel::Info, __VA_ARGS__)
#define LOG_DEBUG(...) ::n64ui::logPrint(::n64ui::LogLevel::Debug, __VA_ARGS__)

}  // namespace n64ui
