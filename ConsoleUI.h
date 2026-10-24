#pragma once

#include "Process.h"
#include "SchedulerCore.h"

#include <string>
#include <vector>

class ConsoleUI {
public:
    static void init();
    static void cleanup();

    static void clear();

    static void printBanner(const std::vector<std::string>& developers,
                            const std::string& lastUpdated);

    static void printMainPrompt();
    static void printScreenPrompt();

    static std::vector<std::string> formatSystemList(const SystemSnapshot& snapshot);
    static std::vector<std::string> formatProcessView(const ProcessSnapshot& process);

    static void printLines(const std::vector<std::string>& lines);

    static bool writeReport(const std::string& path,
                            const std::vector<std::string>& lines,
                            std::string& error);

    static std::string absoluteReportPath(const std::string& filename);

private:
    static void drawAsciiArt();
};
