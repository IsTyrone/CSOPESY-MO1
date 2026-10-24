#pragma once

#include <chrono>
#include <cstdint>
#include <string>

namespace util {

std::string trim(const std::string& s);
std::string toLower(std::string s);

// Strict unsigned-integer parse: digits only, no sign, no trailing garbage.
bool parseU64(const std::string& s, uint64_t& out);

uint16_t clampU16(uint64_t v);
uint8_t  clampU8(uint64_t v);

// MM/DD/YYYY HH:MM:SSAM (12-hour clock, no space before AM/PM).
std::string formatTimestamp(std::chrono::system_clock::time_point tp);

uint64_t envU64(const char* name, uint64_t fallback);

} // namespace util
