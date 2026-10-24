#pragma once

#include <cstdint>
#include <istream>
#include <string>
#include <vector>

struct ConfigLoadResult;

struct Config {
    int      numCpu           = 4;
    std::string scheduler     = "rr";
    uint64_t quantumCycles    = 5;
    uint64_t batchProcessFreq = 1;
    uint64_t minIns           = 1000;
    uint64_t maxIns           = 2000;
    uint64_t delayPerExec     = 0;

    static ConfigLoadResult loadFromFile(const std::string& path);
    static ConfigLoadResult parse(std::istream& in);

    std::vector<std::string> summary() const;
};

struct ConfigLoadResult {
    bool ok = false;
    Config config;
    std::vector<std::string> errors;
    std::vector<std::string> warnings;
};
