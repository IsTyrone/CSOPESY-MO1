#include "CommandInterpreter.h"
#include "ConsoleUI.h"
#include "Util.h"

#include <iostream>
#include <sstream>

namespace {

void splitCommand(const std::string& line, std::string& command, std::string& arguments) {
    command = line;
    arguments.clear();

    const auto split = line.find_first_of(" \t");
    if (split != std::string::npos) {
        command = line.substr(0, split);
        arguments = util::trim(line.substr(split + 1));
    }
}

} // namespace

CommandInterpreter::CommandInterpreter(int tickMs, uint64_t seed)
    : m_tickMs(tickMs),
      m_seed(seed),
      m_developers{
          "Carlo Barreo",
          "Gaibril Kyle",
          "David Javier",
          "Tyrone Lee"
      },
      m_lastUpdated("10-24-2026") {
}

CommandInterpreter::~CommandInterpreter() {
    if (m_scheduler) {
        m_scheduler->stop();
    }
}

bool CommandInterpreter::execute(const std::string& rawLine) {
    m_eof = false;

    const std::string line = util::trim(rawLine);
    if (line.empty()) return false;

    std::string command;
    std::string arguments;
    splitCommand(line, command, arguments);

    const std::string lower = util::toLower(command);

    if (lower == "initialize") {
        handleInitialize();
        return false;
    }

    if (!m_initialized) {
        if (lower == "exit") {
            std::cout << "Goodbye!\n";
            return true;
        }
        std::cout << "Please run 'initialize' first.\n";
        return false;
    }

    if (lower == "exit") {
        if (m_scheduler) m_scheduler->stop();
        std::cout << "Goodbye!\n";
        return true;
    }

    if (lower == "screen") {
        handleScreen(arguments);
        return false;
    }

    if (lower == "scheduler-start" || lower == "scheduler-test") {
        handleSchedulerStart();
        return false;
    }

    if (lower == "scheduler-stop") {
        handleSchedulerStop();
        return false;
    }

    if (lower == "report-util") {
        handleReport();
        return false;
    }

    if (lower == "help") {
        printHelp();
        return false;
    }

    std::cout << "Command not recognized: " << command
              << ". Type 'help' for available commands.\n";
    return false;
}

void CommandInterpreter::handleInitialize() {
    const bool wasInitialized = m_initialized;
    const ConfigLoadResult result = Config::loadFromFile("config.txt");

    if (!result.ok) {
        std::cout << "Failed to load config.txt:\n";
        for (const std::string& error : result.errors) {
            std::cout << "  - " << error << '\n';
        }
        if (m_initialized) {
            std::cout << "Keeping the current configuration and processes.\n";
        }
        return;
    }

    for (const std::string& warning : result.warnings) {
        std::cout << "Warning: " << warning << '\n';
    }

    if (m_scheduler) {
        m_scheduler->stop();
        m_scheduler.reset();
    }

    m_config = result.config;
    m_scheduler = std::make_unique<Scheduler>(m_config, m_tickMs, m_seed);
    m_scheduler->start();
    m_initialized = true;

    if (wasInitialized) {
        std::cout << "Reinitialized. All previous processes were cleared.\n";
    }

    std::cout << "Configuration loaded successfully.\n";
    for (const std::string& line : m_config.summary()) {
        std::cout << line << '\n';
    }
    std::cout << "Scheduler started (tick = " << m_tickMs << " ms).\n";
}

void CommandInterpreter::handleScreen(const std::string& rawArguments) {
    std::istringstream arguments(rawArguments);
    std::string flag;
    arguments >> flag;

    if (flag.empty()) {
        std::cout << "Usage: screen -ls | screen -s <process name> | screen -r <process name>\n";
        return;
    }

    std::string remainder;
    std::getline(arguments, remainder);
    const std::string name = util::trim(remainder);

    if (flag == "-ls") {
        ConsoleUI::printLines(
            ConsoleUI::formatSystemList(m_scheduler->core().snapshot()));
        return;
    }

    if (flag == "-s") {
        if (name.empty()) {
            std::cout << "Usage: screen -s <process name>\n";
            return;
        }

        std::string error;
        if (!m_scheduler->core().createProcess(name, error)) {
            std::cout << error << '\n';
            return;
        }

        attachToProcess(name);
        return;
    }

    if (flag == "-r") {
        if (name.empty()) {
            std::cout << "Usage: screen -r <process name>\n";
            return;
        }

        if (!m_scheduler->core().isActive(name)) {
            std::cout << "Process " << name << " not found.\n";
            return;
        }

        attachToProcess(name);
        return;
    }

    std::cout << "Usage: screen -ls | screen -s <process name> | screen -r <process name>\n";
}

void CommandInterpreter::attachToProcess(const std::string& name) {
    ProcessView view = m_scheduler->core().getProcessView(name);
    if (!view.found) {
        std::cout << "Process " << name << " not found.\n";
        return;
    }

    ConsoleUI::clear();
    ConsoleUI::printLines(ConsoleUI::formatProcessView(view.process));

    std::string line;
    while (true) {
        ConsoleUI::printScreenPrompt();

        if (!std::getline(std::cin, line)) {
            m_eof = true;
            return;
        }

        const std::string trimmed = util::trim(line);
        if (trimmed.empty()) continue;

        const std::string command = util::toLower(trimmed);

        if (command == "exit") {
            ConsoleUI::clear();
            ConsoleUI::printBanner(m_developers, m_lastUpdated);
            return;
        }

        if (command == "process-smi") {
            const ProcessView updated = m_scheduler->core().getProcessView(name);
            if (!updated.found) {
                std::cout << "Process " << name << " not found.\n";
            } else {
                ConsoleUI::printLines(ConsoleUI::formatProcessView(updated.process));
            }
            continue;
        }

        std::cout << "Unknown command. Available: process-smi, exit.\n";
    }
}

void CommandInterpreter::handleSchedulerStart() {
    if (m_scheduler->core().isBatchRunning()) {
        std::cout << "Scheduler is already generating processes.\n";
        return;
    }

    m_scheduler->core().startBatch();
    std::cout << "Scheduler started: one new process every "
              << m_config.batchProcessFreq << " arrival interval(s) (50 ms each).\n";
}

void CommandInterpreter::handleSchedulerStop() {
    if (!m_scheduler->core().isBatchRunning()) {
        std::cout << "Scheduler is not generating processes.\n";
        return;
    }

    m_scheduler->core().stopBatch();
    std::cout << "Scheduler stopped generating new processes.\n";
}

void CommandInterpreter::handleReport() {
    const SystemSnapshot snapshot = m_scheduler->core().snapshot();
    const std::vector<std::string> lines = ConsoleUI::formatSystemList(snapshot);
    const std::string path = ConsoleUI::absoluteReportPath("csopesy-log.txt");

    std::string error;
    if (ConsoleUI::writeReport(path, lines, error)) {
        std::cout << "Report generated at " << path << "!\n";
    } else {
        std::cout << "Failed to generate report: " << error << '\n';
    }
}

void CommandInterpreter::printHelp() {
    std::cout
        << "Available commands:\n"
        << "  initialize                     Load config.txt and start the scheduler\n"
        << "  screen -ls                     Show CPU utilization and processes\n"
        << "  screen -s <process name>       Create and attach to a process\n"
        << "  screen -r <process name>       Attach to an active process\n"
        << "  scheduler-start                Start generating dummy processes\n"
        << "  scheduler-stop                 Stop generating dummy processes\n"
        << "  report-util                    Save the process report to csopesy-log.txt\n"
        << "  exit                           Terminate the emulator\n";
}
