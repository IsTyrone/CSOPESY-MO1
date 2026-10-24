#include "Process.h"
#include "Util.h"

#include <algorithm>
#include <cctype>
#include <utility>

namespace {
constexpr size_t kMaxLogs = 100;
} // namespace

Process::Process(uint64_t id, std::string name, Program program)
    : m_id(id),
      m_name(std::move(name)),
      m_program(std::move(program)),
      m_startTime(std::chrono::system_clock::now()) {
    m_totalLines = m_program.totalLines;
    if (m_totalLines == 0) {
        m_totalLines = static_cast<uint64_t>(m_program.code.size());
    }
}

Process::StepResult Process::executeOneTick(int core, uint64_t delayPerExec) {
    if (m_state == ProcessState::FINISHED) return StepResult::FINISHED;

    // Busy-wait: the process stays on the core but does not advance its PC.
    if (m_delayRemaining > 0) {
        --m_delayRemaining;
        return StepResult::RUNNING;
    }

    if (m_program.code.empty()) {
        finalize();
        return StepResult::FINISHED;
    }

    const Instruction* ins = nullptr;
    if (!m_loops.empty()) {
        LoopFrame& frame = m_loops.back();
        if (frame.body && frame.index < frame.body->size()) {
            ins = &(*frame.body)[frame.index];
        }
    } else {
        if (m_pc >= m_program.code.size()) m_pc = 0;
        ins = &m_program.code[m_pc];
    }

    if (!ins) {
        finalize();
        return StepResult::FINISHED;
    }

    switch (ins->op) {
    case Op::PRINT: {
        std::string message;
        if (ins->b.empty()) {
            message = ins->a.empty() ? ("Hello world from " + m_name + "!") : ins->a;
        } else {
            message = ins->a + std::to_string(variable(ins->b));
        }
        appendLog(core, message);

        completeCurrentInstruction();
        if (m_state == ProcessState::FINISHED) return StepResult::FINISHED;
        m_delayRemaining = delayPerExec;
        return StepResult::RUNNING;
    }

    case Op::DECLARE: {
        m_vars[ins->a] = util::clampU16(resolveOperand(ins->b));

        completeCurrentInstruction();
        if (m_state == ProcessState::FINISHED) return StepResult::FINISHED;
        m_delayRemaining = delayPerExec;
        return StepResult::RUNNING;
    }

    case Op::ADD:
    case Op::SUBTRACT: {
        const uint16_t lhs = resolveOperand(ins->b);
        const uint16_t rhs = resolveOperand(ins->c);

        if (ins->op == Op::ADD) {
            m_vars[ins->a] = util::clampU16(static_cast<uint64_t>(lhs) + rhs);
        } else {
            m_vars[ins->a] = lhs <= rhs ? 0 : static_cast<uint16_t>(lhs - rhs);
        }

        completeCurrentInstruction();
        if (m_state == ProcessState::FINISHED) return StepResult::FINISHED;
        m_delayRemaining = delayPerExec;
        return StepResult::RUNNING;
    }

    case Op::SLEEP: {
        const uint8_t ticks = util::clampU8(resolveOperand(ins->a));

        completeCurrentInstruction();
        if (m_state == ProcessState::FINISHED) return StepResult::FINISHED;

        m_state = ProcessState::WAITING;
        m_sleepRemaining = ticks;
        m_core = -1;
        m_delayRemaining = 0;
        return StepResult::SLEPT;
    }

    case Op::FOR: {
        if (ins->repeats == 0 || ins->body.empty()) {
            completeCurrentInstruction();
            if (m_state == ProcessState::FINISHED) return StepResult::FINISHED;
            m_delayRemaining = delayPerExec;
            return StepResult::RUNNING;
        }

        LoopFrame frame;
        frame.body = &ins->body;
        frame.index = 0;
        frame.remaining = ins->repeats;
        m_loops.push_back(frame);

        m_delayRemaining = delayPerExec;
        return StepResult::RUNNING;
    }
    }

    finalize();
    return StepResult::FINISHED;
}

bool Process::tickSleep() {
    if (m_state != ProcessState::WAITING) return false;

    if (m_sleepRemaining > 0) --m_sleepRemaining;
    if (m_sleepRemaining == 0) {
        m_state = ProcessState::READY;
        return true;
    }
    return false;
}

void Process::completeCurrentInstruction() {
    if (!m_loops.empty()) {
        LoopFrame& frame = m_loops.back();
        if (!frame.body) {
            m_loops.pop_back();
            completeCurrentInstruction();
            return;
        }

        ++frame.index;
        if (frame.index < frame.body->size()) return;

        if (frame.remaining > 1) {
            --frame.remaining;
            frame.index = 0;
            return;
        }

        // The FOR statement itself is now complete.
        m_loops.pop_back();
        completeCurrentInstruction();
        return;
    }

    ++m_linesCompleted;
    ++m_pc;

    if (m_linesCompleted >= m_totalLines) {
        finalize();
        return;
    }

    if (m_program.code.empty()) {
        finalize();
        return;
    }

    if (m_pc >= m_program.code.size()) {
        m_pc = 0;
    }
}

uint16_t Process::resolveOperand(const std::string& token) const {
    if (token.empty()) return 0;

    const bool numeric = std::all_of(
        token.begin(), token.end(),
        [](unsigned char c) { return std::isdigit(c) != 0; });
    if (numeric) {
        uint64_t value = 0;
        if (util::parseU64(token, value)) {
            return util::clampU16(value);
        }
    }

    return variable(token);
}

uint16_t Process::variable(const std::string& name) const {
    const auto it = m_vars.find(name);
    return it == m_vars.end() ? 0 : it->second;
}

uint64_t Process::currentLineNumber() const {
    if (m_totalLines == 0) return 0;
    if (m_state == ProcessState::FINISHED) return m_totalLines;
    return std::min<uint64_t>(m_linesCompleted + 1, m_totalLines);
}

void Process::appendLog(int core, const std::string& message) {
    std::string line = "(" + util::formatTimestamp(std::chrono::system_clock::now()) +
                       ") Core:" + std::to_string(core) + " \"" + message + "\"";
    m_logs.push_back(std::move(line));
    if (m_logs.size() > kMaxLogs) {
        m_logs.pop_front();
    }
}

void Process::finalize() {
    if (m_state == ProcessState::FINISHED) return;

    m_state = ProcessState::FINISHED;
    m_core = -1;
    m_finishTime = std::chrono::system_clock::now();
    m_delayRemaining = 0;
    m_sleepRemaining = 0;
    m_pc = 0;
    m_vars.clear();
    m_loops.clear();
    m_program.code.clear();
    m_program.code.shrink_to_fit();
}

ProcessSnapshot Process::snapshot() const {
    ProcessSnapshot snap;
    snap.id = m_id;
    snap.name = m_name;
    snap.state = m_state;
    snap.startTime = m_startTime;
    snap.finishTime = m_finishTime;
    snap.core = m_core;
    snap.currentLine = currentLineNumber();
    snap.linesOfCode = m_totalLines;
    snap.logs.assign(m_logs.begin(), m_logs.end());
    return snap;
}
