#pragma once

#include "Config.h"
#include "Scheduler.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

class CommandInterpreter {
public:
    CommandInterpreter(int tickMs, uint64_t seed);
    ~CommandInterpreter();

    // Returns true when the application should terminate.
    bool execute(const std::string& line);

    bool eof() const { return m_eof; }

private:
    void handleInitialize();
    void handleScreen(const std::string& arguments);
    void handleSchedulerStart();
    void handleSchedulerStop();
    void handleReport();
    void printHelp();
    void attachToProcess(const std::string& name);

    int m_tickMs;
    uint64_t m_seed;

    bool m_initialized = false;
    bool m_eof = false;

    std::unique_ptr<Scheduler> m_scheduler;
    Config m_config;

    std::vector<std::string> m_developers;
    std::string m_lastUpdated;
};
