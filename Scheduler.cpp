#include "Scheduler.h"

#include <chrono>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

Scheduler::Scheduler(const Config& config, int tickMs, uint64_t seed)
    : m_core(config, seed),
      m_tickMs(tickMs > 0 ? tickMs : kDefaultTickMs) {
}

Scheduler::~Scheduler() {
    stop();
}

void Scheduler::start() {
    if (m_running.exchange(true)) return;

    m_thread = std::thread([this]() {
        using clock = std::chrono::steady_clock;
        auto next = clock::now();
        auto nextArrival = next + std::chrono::milliseconds(kArrivalTickMs);
#ifdef _WIN32
        // Windows' ordinary sleep can round 1 ms up to a system timer tick.
        // A high-resolution waitable timer keeps CPU cycles near the requested rate.
        constexpr DWORD highResolutionTimer = 0x00000002;
        HANDLE timer = CreateWaitableTimerExW(nullptr, nullptr,
                                             highResolutionTimer, TIMER_ALL_ACCESS);
#endif

        while (m_running.load()) {
            const auto tickTime = clock::now();
            const bool arrivalTick = tickTime >= nextArrival;
            if (arrivalTick) {
                // Do not burst-generate missed arrivals after a scheduling stall.
                nextArrival = tickTime + std::chrono::milliseconds(kArrivalTickMs);
            }
            m_core.tickOnce(arrivalTick);

            next += std::chrono::milliseconds(m_tickMs);
            const auto now = clock::now();
            if (next < now) {
                next = now + std::chrono::milliseconds(m_tickMs);
            }
#ifdef _WIN32
            if (timer) {
                const auto remaining = next - clock::now();
                if (remaining > clock::duration::zero()) {
                    LARGE_INTEGER due;
                    due.QuadPart = -(std::chrono::duration_cast<std::chrono::nanoseconds>(
                                        remaining).count() / 100 + 1);
                    if (SetWaitableTimer(timer, &due, 0, nullptr, nullptr, FALSE)) {
                        WaitForSingleObject(timer, INFINITE);
                    } else {
                        std::this_thread::sleep_until(next);
                    }
                }
            } else {
                std::this_thread::sleep_until(next);
            }
#else
            std::this_thread::sleep_until(next);
#endif
        }
#ifdef _WIN32
        if (timer) CloseHandle(timer);
#endif
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
