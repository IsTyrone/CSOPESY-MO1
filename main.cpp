#include "CommandInterpreter.h"
#include "ConsoleUI.h"
#include "Util.h"

#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

namespace {

int parseTickMs(int argc, char** argv, bool& explicitlySet) {
    int tickMs = Scheduler::kDefaultTickMs;
    explicitlySet = false;

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--tick-ms" && i + 1 < argc) {
            tickMs = std::atoi(argv[++i]);
            explicitlySet = true;
        } else if (arg.rfind("--tick-ms=", 0) == 0) {
            tickMs = std::atoi(arg.c_str() + 10);
            explicitlySet = true;
        }
    }

    if (tickMs < 1) tickMs = Scheduler::kDefaultTickMs;
    if (tickMs > 10000) tickMs = 10000;
    return tickMs;
}

} // namespace

int main(int argc, char** argv) {
    bool tickExplicit = false;
    int tickMs = parseTickMs(argc, argv, tickExplicit);

    if (!tickExplicit) {
        const uint64_t envTick = util::envU64("CSOPESY_TICK_MS", 0);
        if (envTick > 0 && envTick <= 10000) {
            tickMs = static_cast<int>(envTick);
        }
    }

    const uint64_t seed = util::envU64("CSOPESY_SEED", 0);

    ConsoleUI::init();

    const std::vector<std::string> developers = {
        "Carlo Barreo",
        "Gaibril Kyle",
        "David Javier",
        "Tyrone Lee"
    };
    ConsoleUI::printBanner(developers, "11-05-2026");

    CommandInterpreter interpreter(tickMs, seed);

    std::string line;
    while (true) {
        ConsoleUI::printMainPrompt();
        if (!std::getline(std::cin, line)) break;
        if (interpreter.execute(line)) break;
        if (interpreter.eof()) break;
    }

    ConsoleUI::cleanup();
    return 0;
}
