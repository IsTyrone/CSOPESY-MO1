#include "ConsoleUI.h"
#include "Util.h"

#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>

#ifdef _WIN32
#include <windows.h>
#ifndef ENABLE_VIRTUAL_TERMINAL_PROCESSING
#define ENABLE_VIRTUAL_TERMINAL_PROCESSING 0x0004
#endif
#endif

namespace {

bool s_vtEnabled = false;

const char* kArt[] = {
    "  ###### #######  ######  ######  ####### ####### ##    ##",
    " ##      ##      ##    ## ##   ## ##      ##       ##  ## ",
    " ##      ####### ##    ## ######  #####   #######   ####  ",
    " ##           ## ##    ## ##      ##           ##    ##   ",
    "  ###### #######  ######  ##      ####### #######    ##   ",
};

std::string blockify(const char* pattern) {
    std::string result;
    for (const char* p = pattern; *p; ++p) {
        if (*p == '#') {
            result += s_vtEnabled ? "\xE2\x96\x88" : "#";
        } else {
            result += *p;
        }
    }
    return result;
}

} // namespace

void ConsoleUI::init() {
#ifdef _WIN32
    HANDLE out = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD mode = 0;
    if (GetConsoleMode(out, &mode)) {
        if (SetConsoleMode(out, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING)) {
            s_vtEnabled = true;
        }
    }
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleTitleA("CSOPESY Emulator");
#else
    s_vtEnabled = true;
#endif
}

void ConsoleUI::cleanup() {
    if (s_vtEnabled) {
        std::cout << "\033[0m";
    }
    std::cout.flush();
}

void ConsoleUI::clear() {
#ifdef _WIN32
    HANDLE out = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_SCREEN_BUFFER_INFO info;
    DWORD written = 0;

    if (GetConsoleScreenBufferInfo(out, &info)) {
        const DWORD cells = static_cast<DWORD>(info.dwSize.X) *
                            static_cast<DWORD>(info.dwSize.Y);
        const COORD home = {0, 0};
        FillConsoleOutputCharacterA(out, ' ', cells, home, &written);
        FillConsoleOutputAttribute(out, info.wAttributes, cells, home, &written);
        SetConsoleCursorPosition(out, home);
        return;
    }
#endif
    std::cout << "\033[2J\033[H" << std::flush;
}

void ConsoleUI::drawAsciiArt() {
    for (const char* row : kArt) {
        const std::string line = blockify(row);
        if (s_vtEnabled) {
            std::cout << "\033[1;36m" << line << "\033[0m\n";
        } else {
            std::cout << line << '\n';
        }
    }
}

void ConsoleUI::printBanner(const std::vector<std::string>& developers,
                            const std::string& lastUpdated) {
    std::cout << "============================================================\n";
    drawAsciiArt();
    std::cout << "\nWelcome to CSOPESY Emulator!\n\n";
    std::cout << "Developers:\n";
    for (const std::string& developer : developers) {
        std::cout << "  " << developer << '\n';
    }
    std::cout << "\nLast updated: " << lastUpdated << '\n';
    std::cout << "============================================================\n\n";
    std::cout.flush();
}

void ConsoleUI::printMainPrompt() {
    std::cout << "root> " << std::flush;
}

void ConsoleUI::printScreenPrompt() {
    std::cout << "root>process=smi " << std::flush;
}

std::vector<std::string> ConsoleUI::formatSystemList(const SystemSnapshot& snapshot) {
    std::vector<std::string> lines;

    lines.push_back("CPU utilization: " + std::to_string(snapshot.cpuUtilization) + "%");
    lines.push_back("Cores used: " + std::to_string(snapshot.coresUsed));
    lines.push_back("Cores available: " + std::to_string(snapshot.coresAvailable));
    lines.push_back("-------------------------------------------");
    lines.push_back("Running processes:");

    if (snapshot.active.empty()) {
        lines.push_back("(none)");
    } else {
        for (const ActiveProcessInfo& process : snapshot.active) {
            std::ostringstream row;
            row << std::left << std::setw(16) << process.name
                << " (" << util::formatTimestamp(process.startTime) << ")";
            row << "  " << (process.core >= 0
                                ? "Core: " + std::to_string(process.core)
                                : std::string("Core: -"));
            row << "  " << process.currentLine << " / " << process.linesOfCode;
            lines.push_back(row.str());
        }
    }

    lines.push_back("");
    lines.push_back("Finished processes:");

    if (snapshot.finished.empty()) {
        lines.push_back("(none)");
    } else {
        for (const FinishedProcessInfo& process : snapshot.finished) {
            std::ostringstream row;
            row << std::left << std::setw(16) << process.name
                << " (" << util::formatTimestamp(process.startTime) << ")";
            row << "  Finished  " << process.linesOfCode
                << " / " << process.linesOfCode;
            lines.push_back(row.str());
        }
    }

    return lines;
}

std::vector<std::string> ConsoleUI::formatProcessView(const ProcessSnapshot& process) {
    std::vector<std::string> lines;

    lines.push_back("Process name: " + process.name);
    lines.push_back("Id: " + std::to_string(process.id));
    lines.push_back("Logs:");

    if (process.logs.empty()) {
        lines.push_back("(no output yet)");
    } else {
        for (const std::string& log : process.logs) {
            lines.push_back(log);
        }
    }

    lines.push_back("");

    if (process.state == ProcessState::FINISHED) {
        lines.push_back("Finished!");
    } else {
        lines.push_back("Current instruction line: " + std::to_string(process.currentLine));
        lines.push_back("Lines of code: " + std::to_string(process.linesOfCode));
    }

    return lines;
}

void ConsoleUI::printLines(const std::vector<std::string>& lines) {
    for (const std::string& line : lines) {
        std::cout << line << '\n';
    }
    std::cout.flush();
}

bool ConsoleUI::writeReport(const std::string& path,
                            const std::vector<std::string>& lines,
                            std::string& error) {
    std::ofstream out(path, std::ios::trunc);
    if (!out.is_open()) {
        error = "Could not open " + path + " for writing.";
        return false;
    }

    for (const std::string& line : lines) {
        out << line << '\n';
    }

    if (!out) {
        error = "Failed while writing " + path + ".";
        return false;
    }

    return true;
}

std::string ConsoleUI::absoluteReportPath(const std::string& filename) {
    try {
        return std::filesystem::absolute(filename).generic_string();
    } catch (...) {
        return filename;
    }
}
