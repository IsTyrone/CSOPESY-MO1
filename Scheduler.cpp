#include "Scheduler.h"

#include <chrono>

Scheduler::Scheduler(const Config& config, int tickMs, uint64_t seed)
    : m_core(config, seed),
      m_tickMs(tickMs > 0 ? tickMs : 50) {
}

Scheduler::~Scheduler() {
    stop();
}

void Scheduler::start() {
    if (m_running.exchange(true)) return;

    m_thread = std::thread([this]() {
        using clock = std::chrono::steady_clock;
        auto next = clock::now();

        while (m_running.load()) {
            m_core.tickOnce();

            next += std::chrono::milliseconds(m_tickMs);
            const auto now = clock::now();
            if (next < now) {
                next = now + std::chrono::milliseconds(m_tickMs);
            }
            std::this_thread::sleep_until(next);
        }
    });
}

void Scheduler::stop() {
    const bool wasRunning = m_running.exchange(false);
    if (m_thread.joinable()) {
        m_thread.join();
    } else if (!wasRunning) {
        // Nothing to do.
    }
}
