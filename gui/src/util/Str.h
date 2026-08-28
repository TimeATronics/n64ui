// Small string helpers. No std::filesystem dependency; plain C strings + std::string.
#pragma once

#include <string>
#include <vector>

namespace n64ui {

std::string strFormat(const char* fmt, ...);
std::string strTrim(const std::string& s);
std::vector<std::string> strSplit(const std::string& s, char sep);
bool strStartsWith(const std::string& s, const std::string& prefix);
bool strEndsWith(const std::string& s, const std::string& suffix);
std::string strLower(const std::string& s);
std::string strBaseName(const std::string& path);
std::string strDirName(const std::string& path);
bool strIsNumber(const std::string& s);

}  // namespace n64ui
