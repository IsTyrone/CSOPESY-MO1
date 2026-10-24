#include "Config.h"
#include "Util.h"

#include <fstream>
#include <sstream>

namespace {

constexpr uint64_t kMaxU32Inc = 4294967296ULL; // 2^32

std::string stripQuotes(const std::string& value) {
    if (value.size() >= 2 && value.front() == '"' && value.back() == '"') {
        return value.substr(1, value.size() - 2);
    }
    return value;
}

bool parseField(const std::string& key, const std::string& value,
                Config& cfg, std::vector<std::string>& errors) {
    uint64_t number = 0;

    if (key == "num-cpu") {
        if (!util::parseU64(value, number)) {
            errors.push_back("num-cpu: not a valid unsigned integer: " + value);
            return true;
        }
        if (number < 1 || number > 128) {
            errors.push_back("num-cpu: must be in [1, 128] (got " + value + ")");
            return true;
        }
        cfg.numCpu = static_cast<int>(number);
        return true;
    }

    if (key == "scheduler") {
        const std::string sched = util::toLower(stripQuotes(value));
        if (sched != "fcfs" && sched != "rr") {
            errors.push_back("scheduler: must be \"fcfs\" or \"rr\" (got " + value + ")");
            return true;
        }
        cfg.scheduler = sched;
        return true;
    }

    struct U64Field {
        const char* name;
        uint64_t Config::*member;
        uint64_t minimum;
    };
    static const U64Field fields[] = {
        {"quantum-cycles",     &Config::quantumCycles,    1},
        {"batch-process-freq", &Config::batchProcessFreq, 1},
        {"min-ins",            &Config::minIns,           1},
        {"max-ins",            &Config::maxIns,           1},
        {"delay-per-exec",     &Config::delayPerExec,     0},
        {"delays-per-exec",    &Config::delayPerExec,     0},
    };

    for (const auto& field : fields) {
        if (key == field.name) {
            if (!util::parseU64(value, number)) {
                errors.push_back(std::string(field.name) +
                                 ": not a valid unsigned integer: " + value);
                return true;
            }
            if (number < field.minimum || number > kMaxU32Inc) {
                errors.push_back(std::string(field.name) + ": must be in [" +
                                 std::to_string(field.minimum) + ", 2^32] (got " +
                                 value + ")");
                return true;
            }
            cfg.*(field.member) = number;
            return true;
        }
    }

    return false; // unknown key
}

} // namespace

ConfigLoadResult Config::loadFromFile(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        ConfigLoadResult result;
        result.errors.push_back("Could not open " + path +
                                " (run the program from the folder that contains config.txt).");
        return result;
    }
    return parse(file);
}

ConfigLoadResult Config::parse(std::istream& in) {
    Config cfg;
    ConfigLoadResult result;
    std::string raw;
    int lineNumber = 0;

    while (std::getline(in, raw)) {
        ++lineNumber;
        std::string line = util::trim(raw);

        const auto comment = line.find('#');
        if (comment != std::string::npos) line = util::trim(line.substr(0, comment));
        if (line.empty()) continue;

        const auto split = line.find_first_of(" \t");
        if (split == std::string::npos) {
            result.errors.push_back("line " + std::to_string(lineNumber) +
                                    ": expected '<parameter> <value>'");
            continue;
        }

        const std::string key = util::toLower(util::trim(line.substr(0, split)));
        const std::string value = util::trim(line.substr(split));

        if (key.empty() || value.empty()) {
            result.errors.push_back("line " + std::to_string(lineNumber) +
                                    ": expected '<parameter> <value>'");
            continue;
        }

        if (!parseField(key, value, cfg, result.errors)) {
            result.warnings.push_back("line " + std::to_string(lineNumber) +
                                      ": unknown parameter '" + key + "' (ignored)");
        }
    }

    if (cfg.minIns > cfg.maxIns) {
        result.errors.push_back("min-ins must be less than or equal to max-ins");
    }

    result.config = cfg;
    result.ok = result.errors.empty();
    return result;
}

std::vector<std::string> Config::summary() const {
    return {
        "  num-cpu:            " + std::to_string(numCpu),
        "  scheduler:          " + scheduler,
        "  quantum-cycles:     " + std::to_string(quantumCycles),
        "  batch-process-freq: " + std::to_string(batchProcessFreq),
        "  min-ins:            " + std::to_string(minIns),
        "  max-ins:            " + std::to_string(maxIns),
        "  delay-per-exec:     " + std::to_string(delayPerExec),
    };
}
