#pragma once

#include "Instruction.h"

#include <chrono>
#include <cstdint>
#include <deque>
#include <string>
#include <unordered_map>
#include <vector>

enum class ProcessState {
    READY,
    RUNNING,
    WAITING,
    FINISHED
};

struct ProcessSnapshot {
    uint64_t id = 0;
    std::string name;
    ProcessState state = ProcessState::READY;
    std::chrono::system_clock::time_point startTime;
    std::chrono::system_clock::time_point finishTime;
    int core = -1;
    uint64_t currentLine = 0;
    uint64_t linesOfCode = 0;
    std::vector<std::string> logs;
};

class Process {
public:
    enum class StepResult {
        RUNNING,
        SLEPT,
        FINISHED
    };

    Process(uint64_t id, std::string name, Program program);

    StepResult executeOneTick(int core, uint64_t delayPerExec);

    // Decrements a WAITING process's sleep counter. Returns true when it
    // becomes READY and should be put back on the ready queue.
    bool tickSleep();

    void finalize();

    uint64_t id() const { return m_id; }
    const std::string& name() const { return m_name; }

    ProcessState state() const { return m_state; }
    void setState(ProcessState state) { m_state = state; }

    int core() const { return m_core; }
    void setCore(int core) { m_core = core; }

    uint64_t delayRemaining() const { return m_delayRemaining; }
    uint64_t sleepRemaining() const { return m_sleepRemaining; }
    void setSleepRemaining(uint64_t ticks) { m_sleepRemaining = ticks; }

    uint64_t linesCompleted() const { return m_linesCompleted; }
    uint64_t linesOfCode() const { return m_totalLines; }
    uint64_t currentLineNumber() const;
    bool finished() const { return m_state == ProcessState::FINISHED; }

    uint16_t variable(const std::string& name) const;
    const std::deque<std::string>& logs() const { return m_logs; }

    std::chrono::system_clock::time_point startTime() const { return m_startTime; }
    std::chrono::system_clock::time_point finishTime() const { return m_finishTime; }

    ProcessSnapshot snapshot() const;

private:
    struct LoopFrame {
        const std::vector<Instruction>* body = nullptr;
        size_t index = 0;
        uint64_t remaining = 0;
    };

    void completeCurrentInstruction();
    uint16_t resolveOperand(const std::string& token) const;
    void appendLog(int core, const std::string& message);

    uint64_t m_id;
    std::string m_name;
    ProcessState m_state = ProcessState::READY;
    int m_core = -1;

    Program m_program;
    uint64_t m_totalLines = 0;
    size_t m_pc = 0;
    uint64_t m_linesCompleted = 0;
    std::vector<LoopFrame> m_loops;

    std::unordered_map<std::string, uint16_t> m_vars;
    uint64_t m_delayRemaining = 0;
    uint64_t m_sleepRemaining = 0;

    std::deque<std::string> m_logs;
    std::chrono::system_clock::time_point m_startTime;
    std::chrono::system_clock::time_point m_finishTime;
};
