#include "SchedulerCore.h"
#include "Instruction.h"
#include "Util.h"

#include <algorithm>
#include <iomanip>
#include <sstream>
#include <utility>

SchedulerCore::SchedulerCore(const Config& config, uint64_t seed)
    : m_config(config) {
    if (m_config.numCpu < 1) m_config.numCpu = 1;
    if (m_config.numCpu > 128) m_config.numCpu = 128;
    if (m_config.quantumCycles < 1) m_config.quantumCycles = 1;
    if (m_config.batchProcessFreq < 1) m_config.batchProcessFreq = 1;

    m_cores.resize(static_cast<size_t>(m_config.numCpu));

    if (seed == 0) {
        std::random_device device;
        m_rng.seed(device());
    } else {
        m_rng.seed(seed);
    }
}

void SchedulerCore::tickOnce() {
    std::lock_guard<std::mutex> lock(m_mutex);

    ++m_tickCount;

    if (m_batchRunning &&
        m_tickCount % m_config.batchProcessFreq == 0) {
        generateProcessLocked();
    }

    wakeSleepingLocked();
    dispatchLocked();
    executeLocked();
}

void SchedulerCore::generateProcessLocked() {
    const std::string name = makeBatchNameLocked();
    Program program = generateProgram(m_config.minIns, m_config.maxIns, m_rng);
    std::string error;
    addProcessLocked(name, std::move(program), error);
}

void SchedulerCore::wakeSleepingLocked() {
    for (const auto& process : m_all) {
        if (process->state() == ProcessState::WAITING) {
            if (process->tickSleep()) {
                m_ready.push_back(process);
            }
        }
    }
}

void SchedulerCore::dispatchLocked() {
    for (int core = 0; core < m_config.numCpu; ++core) {
        if (m_cores[static_cast<size_t>(core)].proc) continue;

        while (!m_ready.empty()) {
            std::shared_ptr<Process> process = m_ready.front();
            m_ready.pop_front();

            if (!process || process->state() != ProcessState::READY) continue;

            process->setState(ProcessState::RUNNING);
            process->setCore(core);
            m_cores[static_cast<size_t>(core)].proc = process;
            m_cores[static_cast<size_t>(core)].quantumUsed = 0;
            break;
        }
    }
}

void SchedulerCore::executeLocked() {
    for (int core = 0; core < m_config.numCpu; ++core) {
        Core& slot = m_cores[static_cast<size_t>(core)];
        if (!slot.proc) continue;

        const std::shared_ptr<Process> process = slot.proc;
        if (process->state() != ProcessState::RUNNING) {
            slot.proc.reset();
            slot.quantumUsed = 0;
            continue;
        }

        const Process::StepResult result =
            process->executeOneTick(core, m_config.delayPerExec);

        if (result == Process::StepResult::FINISHED ||
            result == Process::StepResult::SLEPT) {
            slot.proc.reset();
            slot.quantumUsed = 0;
            continue;
        }

        ++slot.quantumUsed;

        if (m_config.scheduler == "rr" &&
            slot.quantumUsed >= m_config.quantumCycles &&
            process->state() == ProcessState::RUNNING) {
            process->setState(ProcessState::READY);
            process->setCore(-1);
            slot.proc.reset();
            slot.quantumUsed = 0;
            m_ready.push_back(process);
        }
    }
}

bool SchedulerCore::createProcess(const std::string& rawName, std::string& error) {
    std::lock_guard<std::mutex> lock(m_mutex);

    const std::string name = util::trim(rawName);
    if (name.empty()) {
        error = "Process name cannot be empty.";
        return false;
    }
    if (m_byName.find(name) != m_byName.end()) {
        error = "Process '" + name + "' already exists.";
        return false;
    }

    Program program = generateProgram(m_config.minIns, m_config.maxIns, m_rng);
    return addProcessLocked(name, std::move(program), error);
}

bool SchedulerCore::addProcess(const std::string& rawName, Program program,
                               std::string& error) {
    std::lock_guard<std::mutex> lock(m_mutex);
    return addProcessLocked(util::trim(rawName), std::move(program), error);
}

bool SchedulerCore::addProcessLocked(const std::string& name, Program program,
                                     std::string& error) {
    if (name.empty()) {
        error = "Process name cannot be empty.";
        return false;
    }
    if (m_byName.find(name) != m_byName.end()) {
        error = "Process '" + name + "' already exists.";
        return false;
    }
    if (program.code.empty()) {
        error = "Program has no instructions.";
        return false;
    }
    if (program.totalLines == 0) {
        program.totalLines = static_cast<uint64_t>(program.code.size());
    }

    auto process = std::make_shared<Process>(m_nextId++, name, std::move(program));
    m_all.push_back(process);
    m_byName.emplace(name, process);
    m_ready.push_back(process);
    return true;
}

bool SchedulerCore::hasName(const std::string& name) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_byName.find(name) != m_byName.end();
}

bool SchedulerCore::isActive(const std::string& name) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    const auto it = m_byName.find(name);
    return it != m_byName.end() &&
           it->second->state() != ProcessState::FINISHED;
}

ProcessView SchedulerCore::getProcessView(const std::string& name) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    ProcessView view;
    const auto it = m_byName.find(name);
    if (it == m_byName.end()) return view;

    view.found = true;
    view.process = it->second->snapshot();
    return view;
}

SystemSnapshot SchedulerCore::snapshot() const {
    std::lock_guard<std::mutex> lock(m_mutex);

    SystemSnapshot snap;
    snap.numCpu = m_config.numCpu;

    int used = 0;
    for (const Core& core : m_cores) {
        if (core.proc) ++used;
    }
    snap.coresUsed = used;
    snap.coresAvailable = m_config.numCpu - used;
    snap.cpuUtilization = m_config.numCpu > 0
                              ? (used * 100) / m_config.numCpu
                              : 0;

    for (const auto& process : m_all) {
        if (process->state() == ProcessState::FINISHED) {
            FinishedProcessInfo info;
            info.id = process->id();
            info.name = process->name();
            info.startTime = process->startTime();
            info.finishTime = process->finishTime();
            info.linesOfCode = process->linesOfCode();
            snap.finished.push_back(std::move(info));
        } else {
            ActiveProcessInfo info;
            info.id = process->id();
            info.name = process->name();
            info.state = process->state();
            info.startTime = process->startTime();
            info.core = process->core();
            info.currentLine = process->currentLineNumber();
            info.linesOfCode = process->linesOfCode();
            snap.active.push_back(std::move(info));
        }
    }

    std::sort(snap.active.begin(), snap.active.end(),
              [](const ActiveProcessInfo& a, const ActiveProcessInfo& b) {
                  return a.id < b.id;
              });
    std::sort(snap.finished.begin(), snap.finished.end(),
              [](const FinishedProcessInfo& a, const FinishedProcessInfo& b) {
                  return a.id < b.id;
              });

    return snap;
}

void SchedulerCore::startBatch() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_batchRunning = true;
}

void SchedulerCore::stopBatch() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_batchRunning = false;
}

bool SchedulerCore::isBatchRunning() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_batchRunning;
}

uint64_t SchedulerCore::tickCount() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_tickCount;
}

std::string SchedulerCore::makeBatchNameLocked() {
    for (;;) {
        std::ostringstream name;
        name << 'p' << std::setw(2) << std::setfill('0') << m_nextProcessNumber++;
        const std::string candidate = name.str();
        if (m_byName.find(candidate) == m_byName.end()) {
            return candidate;
        }
    }
}

std::shared_ptr<Process> SchedulerCore::findLocked(const std::string& name) const {
    const auto it = m_byName.find(name);
    return it == m_byName.end() ? nullptr : it->second;
}
