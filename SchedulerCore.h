#pragma once

#include "Config.h"
#include "Process.h"

#include <chrono>
#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <random>
#include <string>
#include <unordered_map>
#include <vector>

struct ActiveProcessInfo {
    uint64_t id = 0;
    std::string name;
    ProcessState state = ProcessState::READY;
    std::chrono::system_clock::time_point startTime;
    int core = -1;
    uint64_t currentLine = 0;
    uint64_t linesOfCode = 0;
};

struct FinishedProcessInfo {
    uint64_t id = 0;
    std::string name;
    std::chrono::system_clock::time_point startTime;
    std::chrono::system_clock::time_point finishTime;
    uint64_t linesOfCode = 0;
};

struct SystemSnapshot {
    int numCpu = 0;
    int coresUsed = 0;
    int coresAvailable = 0;
    int cpuUtilization = 0;
    std::vector<ActiveProcessInfo> active;
    std::vector<FinishedProcessInfo> finished;
};

struct ProcessView {
    bool found = false;
    ProcessSnapshot process;
};

class SchedulerCore {
public:
    explicit SchedulerCore(const Config& config, uint64_t seed = 0);

    void tickOnce();

    bool createProcess(const std::string& name, std::string& error);
    bool addProcess(const std::string& name, Program program, std::string& error);

    bool hasName(const std::string& name) const;
    bool isActive(const std::string& name) const;
    ProcessView getProcessView(const std::string& name) const;

    SystemSnapshot snapshot() const;

    void startBatch();
    void stopBatch();
    bool isBatchRunning() const;
    uint64_t tickCount() const;

    const Config& config() const { return m_config; }

private:
    struct Core {
        std::shared_ptr<Process> proc;
        uint64_t quantumUsed = 0;
    };

    void generateProcessLocked();
    void wakeSleepingLocked();
    void preemptExpiredLocked();
    void dispatchLocked();
    void executeLocked();

    std::string makeBatchNameLocked();
    bool addProcessLocked(const std::string& name, Program program, std::string& error);
    std::shared_ptr<Process> findLocked(const std::string& name) const;

    Config m_config;
    std::mt19937_64 m_rng;
    mutable std::mutex m_mutex;

    std::vector<Core> m_cores;
    std::deque<std::shared_ptr<Process>> m_ready;
    std::vector<std::shared_ptr<Process>> m_all;
    std::unordered_map<std::string, std::shared_ptr<Process>> m_byName;

    bool m_batchRunning = false;
    uint64_t m_tickCount = 0;
    uint64_t m_nextId = 1;
    uint64_t m_nextProcessNumber = 1;
};
