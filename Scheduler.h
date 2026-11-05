#pragma once

#include "Config.h"
#include "SchedulerCore.h"

#include <atomic>
#include <cstdint>
#include <thread>

class Scheduler {
public:
    static constexpr int kDefaultTickMs = 1;
    static constexpr int kArrivalTickMs = 50;

    Scheduler(const Config& config, int tickMs = kDefaultTickMs, uint64_t seed = 0);
    ~Scheduler();

    Scheduler(const Scheduler&) = delete;
    Scheduler& operator=(const Scheduler&) = delete;

    void start();
    void stop();

    SchedulerCore& core() { return m_core; }
    const SchedulerCore& core() const { return m_core; }

    int tickMs() const { return m_tickMs; }

private:
    SchedulerCore m_core;
    int m_tickMs;
    std::atomic<bool> m_running{false};
    std::thread m_thread;
};
