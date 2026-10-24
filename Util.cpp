#include "Util.h"

#include <cctype>
#include <cstdlib>
#include <ctime>
#include <limits>

namespace util {

std::string trim(const std::string& s) {
    const auto begin = s.find_first_not_of(" \t\r\n");
    if (begin == std::string::npos) return "";
    const auto end = s.find_last_not_of(" \t\r\n");
    return s.substr(begin, end - begin + 1);
}

std::string toLower(std::string s) {
    for (char& c : s) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return s;
}

bool parseU64(const std::string& s, uint64_t& out) {
    if (s.empty()) return false;
    for (char c : s) {
        if (!std::isdigit(static_cast<unsigned char>(c))) return false;
    }
    try {
        size_t pos = 0;
        const unsigned long long value = std::stoull(s, &pos);
        if (pos != s.size()) return false;
        out = static_cast<uint64_t>(value);
        return true;
    } catch (...) {
        return false;
    }
}

uint16_t clampU16(uint64_t v) {
    return v > std::numeric_limits<uint16_t>::max()
               ? std::numeric_limits<uint16_t>::max()
               : static_cast<uint16_t>(v);
}

uint8_t clampU8(uint64_t v) {
    return v > std::numeric_limits<uint8_t>::max()
               ? std::numeric_limits<uint8_t>::max()
               : static_cast<uint8_t>(v);
}

std::string formatTimestamp(std::chrono::system_clock::time_point tp) {
    const std::time_t t = std::chrono::system_clock::to_time_t(tp);
    std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif
    char buffer[64] = {0};
    std::strftime(buffer, sizeof(buffer), "%m/%d/%Y %I:%M:%S%p", &tm);
    return std::string(buffer);
}

uint64_t envU64(const char* name, uint64_t fallback) {
    const char* raw = std::getenv(name);
    if (!raw) return fallback;
    uint64_t value = 0;
    if (!parseU64(trim(raw), value)) return fallback;
    return value;
}

} // namespace util
