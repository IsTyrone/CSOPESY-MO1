#include "Config.h"
#include "ConsoleUI.h"
#include "Instruction.h"
#include "Process.h"
#include "SchedulerCore.h"
#include "Util.h"

#include <algorithm>
#include <cstdint>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace {

int g_checks = 0;
int g_failures = 0;

template <typename A, typename B>
void checkEq(const A& actual, const B& expected, const char* expression, int line) {
    ++g_checks;
    if (!(actual == expected)) {
        std::cerr << "FAIL " << line << ": " << expression
                  << "  expected " << expected << ", got " << actual << '\n';
        ++g_failures;
    }
}

#define CHECK(condition)                                                        \
    do {                                                                        \
        ++g_checks;                                                             \
        if (!(condition)) {                                                     \
            std::cerr << "FAIL " << __LINE__ << ": " << #condition << '\n';     \
            ++g_failures;                                                       \
        }                                                                       \
    } while (false)

#define CHECK_EQ(actual, expected) checkEq((actual), (expected), #actual, __LINE__)

Instruction declare(const std::string& var, const std::string& value) {
    Instruction ins;
    ins.op = Op::DECLARE;
    ins.a = var;
    ins.b = value;
    return ins;
}

Instruction add(const std::string& dest, const std::string& lhs, const std::string& rhs) {
    Instruction ins;
    ins.op = Op::ADD;
    ins.a = dest;
    ins.b = lhs;
    ins.c = rhs;
    return ins;
}

Instruction subtract(const std::string& dest, const std::string& lhs, const std::string& rhs) {
    Instruction ins;
    ins.op = Op::SUBTRACT;
    ins.a = dest;
    ins.b = lhs;
    ins.c = rhs;
    return ins;
}

Instruction printDefault() {
    Instruction ins;
    ins.op = Op::PRINT;
    return ins;
}

Instruction printVar(const std::string& prefix, const std::string& var) {
    Instruction ins;
    ins.op = Op::PRINT;
    ins.a = prefix;
    ins.b = var;
    return ins;
}

Instruction printLiteral(const std::string& message) {
    Instruction ins;
    ins.op = Op::PRINT;
    ins.a = message;
    return ins;
}

Instruction sleepFor(const std::string& ticks) {
    Instruction ins;
    ins.op = Op::SLEEP;
    ins.a = ticks;
    return ins;
}

Instruction loop(uint64_t repeats, std::vector<Instruction> body) {
    Instruction ins;
    ins.op = Op::FOR;
    ins.repeats = repeats;
    ins.body = std::move(body);
    return ins;
}

Program program(std::vector<Instruction> code, uint64_t totalLines = 0) {
    Program p;
    p.code = std::move(code);
    p.totalLines = totalLines == 0 ? static_cast<uint64_t>(p.code.size()) : totalLines;
    return p;
}

ConfigLoadResult parseConfig(const std::string& text) {
    std::istringstream in(text);
    return Config::parse(in);
}

void runProcess(Process& process, int ticks, uint64_t delay = 0) {
    process.setState(ProcessState::RUNNING);
    for (int i = 0; i < ticks && !process.finished(); ++i) {
        process.executeOneTick(0, delay);
    }
}

void runUntilLines(Process& process, uint64_t target, int maxTicks = 1000) {
    process.setState(ProcessState::RUNNING);
    for (int i = 0; i < maxTicks && !process.finished(); ++i) {
        if (process.linesCompleted() >= target) break;
        process.executeOneTick(0, 0);
    }
}

void testConfigParsing() {
    const auto valid = parseConfig(
        "num-cpu 2\n"
        "scheduler \"rr\"\n"
        "quantum-cycles 3\n"
        "batch-process-freq 4\n"
        "min-ins 10\n"
        "max-ins 20\n"
        "delay-per-exec 1\n");

    CHECK(valid.ok);
    CHECK_EQ(valid.config.numCpu, 2);
    CHECK_EQ(valid.config.scheduler, std::string("rr"));
    CHECK_EQ(valid.config.quantumCycles, static_cast<uint64_t>(3));
    CHECK_EQ(valid.config.batchProcessFreq, static_cast<uint64_t>(4));
    CHECK_EQ(valid.config.minIns, static_cast<uint64_t>(10));
    CHECK_EQ(valid.config.maxIns, static_cast<uint64_t>(20));
    CHECK_EQ(valid.config.delayPerExec, static_cast<uint64_t>(1));

    const auto alias = parseConfig("delays-per-exec 7\n");
    CHECK(alias.ok);
    CHECK_EQ(alias.config.delayPerExec, static_cast<uint64_t>(7));

    const auto comments = parseConfig(
        "num-cpu 3 # three cores\n"
        "scheduler \"fcfs\"\n");
    CHECK(comments.ok);
    CHECK_EQ(comments.config.numCpu, 3);
    CHECK_EQ(comments.config.scheduler, std::string("fcfs"));

    const auto defaults = parseConfig("");
    CHECK(defaults.ok);
    CHECK_EQ(defaults.config.numCpu, 4);
    CHECK_EQ(defaults.config.scheduler, std::string("rr"));
    CHECK_EQ(defaults.config.minIns, static_cast<uint64_t>(1000));
    CHECK_EQ(defaults.config.maxIns, static_cast<uint64_t>(2000));

    const auto unknown = parseConfig("num-cpu 2\nmystery 9\n");
    CHECK(unknown.ok);
    CHECK_EQ(unknown.config.numCpu, 2);
    CHECK_EQ(unknown.warnings.size(), static_cast<size_t>(1));

    const auto maxRange = parseConfig(
        "min-ins 4294967296\n"
        "max-ins 4294967296\n");
    CHECK(maxRange.ok);
    CHECK_EQ(maxRange.config.minIns, static_cast<uint64_t>(4294967296ULL));

    CHECK(!parseConfig("num-cpu 0\n").ok);
    CHECK(!parseConfig("num-cpu 129\n").ok);
    CHECK(!parseConfig("num-cpu abc\n").ok);
    CHECK(!parseConfig("scheduler \"priority\"\n").ok);
    CHECK(!parseConfig("quantum-cycles 0\n").ok);
    CHECK(!parseConfig("batch-process-freq 0\n").ok);
    CHECK(!parseConfig("min-ins 0\n").ok);
    CHECK(!parseConfig("min-ins 50\nmax-ins 10\n").ok);
    CHECK(!parseConfig("delay-per-exec 4294967297\n").ok);
    CHECK(!parseConfig("num-cpu\n").ok);
}

void testArithmeticAndPrint() {
    Process process(1, "test", program({
        declare("x", "5"),
        declare("y", "10"),
        add("x", "y", "3"),
        subtract("x", "20", "y"),
        printDefault()
    }));

    runProcess(process, 4);
    CHECK(!process.finished());
    CHECK_EQ(static_cast<int>(process.variable("x")), 10);
    CHECK_EQ(process.linesCompleted(), static_cast<uint64_t>(4));

    runProcess(process, 1);
    CHECK(process.finished());
    CHECK_EQ(process.logs().size(), static_cast<size_t>(1));
    CHECK(process.logs().back().find("Hello world from test!") != std::string::npos);
    CHECK_EQ(process.currentLineNumber(), static_cast<uint64_t>(5));
    CHECK_EQ(process.linesOfCode(), static_cast<uint64_t>(5));
}

void testClamping() {
    Process upper(2, "upper", program({
        declare("x", "70000"),
        add("x", "65535", "1"),
        printDefault()
    }));
    runProcess(upper, 2);
    CHECK(!upper.finished());
    CHECK_EQ(static_cast<int>(upper.variable("x")), 65535);
    runProcess(upper, 1);
    CHECK(upper.finished());

    Process lower(3, "lower", program({
        declare("x", "1"),
        subtract("x", "0", "1"),
        subtract("x", "1", "1"),
        printDefault()
    }));
    runProcess(lower, 3);
    CHECK(!lower.finished());
    CHECK_EQ(static_cast<int>(lower.variable("x")), 0);
    runProcess(lower, 1);
    CHECK(lower.finished());

    Process undeclared(4, "undeclared", program({
        add("x", "unknown", "5"),
        printDefault()
    }));
    runProcess(undeclared, 1);
    CHECK(!undeclared.finished());
    CHECK_EQ(static_cast<int>(undeclared.variable("x")), 5);
    runProcess(undeclared, 1);
    CHECK(undeclared.finished());
}

void testPrintVariants() {
    Process process(5, "printer", program({
        declare("x", "42"),
        printVar("Value from: ", "x"),
        printLiteral("literal message"),
        printDefault()
    }));

    runProcess(process, 6);

    CHECK(process.finished());
    CHECK_EQ(process.logs().size(), static_cast<size_t>(3));
    CHECK(process.logs()[0].find("\"Value from: 42\"") != std::string::npos);
    CHECK(process.logs()[1].find("\"literal message\"") != std::string::npos);
    CHECK(process.logs()[2].find("Hello world from printer!") != std::string::npos);
}

void testForLoops() {
    Process single(6, "single", program({
        declare("x", "0"),
        loop(3, {add("x", "x", "1")}),
        printDefault()
    }));

    runUntilLines(single, 2);
    CHECK(!single.finished());
    CHECK_EQ(static_cast<int>(single.variable("x")), 3);
    runProcess(single, 2);
    CHECK(single.finished());
    CHECK_EQ(single.currentLineNumber(), static_cast<uint64_t>(3));

    Process nested(7, "nested", program({
        declare("x", "0"),
        loop(2, {
            loop(2, {add("x", "x", "1")})
        }),
        printDefault()
    }));

    runUntilLines(nested, 2);
    CHECK(!nested.finished());
    CHECK_EQ(static_cast<int>(nested.variable("x")), 4);
    runProcess(nested, 2);
    CHECK(nested.finished());

    Process triple(8, "triple", program({
        declare("x", "0"),
        loop(2, {
            loop(2, {
                loop(2, {add("x", "x", "1")})
            })
        }),
        printDefault()
    }));

    runUntilLines(triple, 2);
    CHECK(!triple.finished());
    CHECK_EQ(static_cast<int>(triple.variable("x")), 8);
    runProcess(triple, 2);
    CHECK(triple.finished());
}

void testTemplateRepeat() {
    Process process(9, "template", program({
        add("x", "x", "1")
    }, 5));

    runProcess(process, 4);
    CHECK(!process.finished());
    CHECK_EQ(static_cast<int>(process.variable("x")), 4);

    runProcess(process, 1);
    CHECK(process.finished());
    CHECK_EQ(process.currentLineNumber(), static_cast<uint64_t>(5));
}

void testLogCap() {
    Process process(10, "logs", program({
        printDefault()
    }, 250));

    runProcess(process, 260);

    CHECK(process.finished());
    CHECK_EQ(process.logs().size(), static_cast<size_t>(100));
}

void testProcessSleep() {
    Process process(11, "sleeper", program({
        sleepFor("2"),
        add("x", "x", "1"),
        printDefault()
    }));

    process.setState(ProcessState::RUNNING);
    const auto first = process.executeOneTick(0, 0);
    CHECK(first == Process::StepResult::SLEPT);
    CHECK(process.state() == ProcessState::WAITING);
    CHECK_EQ(process.sleepRemaining(), static_cast<uint64_t>(2));
    CHECK_EQ(process.core(), -1);
    CHECK_EQ(process.linesCompleted(), static_cast<uint64_t>(1));

    CHECK(!process.tickSleep());
    CHECK_EQ(process.sleepRemaining(), static_cast<uint64_t>(1));
    CHECK(process.tickSleep());
    CHECK(process.state() == ProcessState::READY);
    CHECK_EQ(process.sleepRemaining(), static_cast<uint64_t>(0));

    process.setState(ProcessState::RUNNING);
    process.executeOneTick(0, 0);

    CHECK(!process.finished());
    CHECK_EQ(static_cast<int>(process.variable("x")), 1);

    process.executeOneTick(0, 0);
    CHECK(process.finished());
}

void testGeneratedProgram() {
    std::mt19937_64 rng(12345);

    const Program small = generateProgram(1, 1, rng);
    CHECK_EQ(small.totalLines, static_cast<uint64_t>(1));
    CHECK_EQ(small.code.size(), static_cast<size_t>(1));

    const Program normal = generateProgram(1000, 1000, rng);
    CHECK_EQ(normal.totalLines, static_cast<uint64_t>(1000));
    CHECK(normal.code.size() <= 128);
    CHECK(!normal.code.empty());
    CHECK(std::any_of(normal.code.begin(), normal.code.end(),
                      [](const Instruction& ins) { return ins.op == Op::PRINT; }));

    const Program huge = generateProgram(4294967296ULL, 4294967296ULL, rng);
    CHECK_EQ(huge.totalLines, static_cast<uint64_t>(4294967296ULL));
    CHECK(huge.code.size() <= 128);
}

Config baseSchedulerConfig() {
    Config cfg;
    cfg.numCpu = 1;
    cfg.scheduler = "fcfs";
    cfg.quantumCycles = 100;
    cfg.batchProcessFreq = 1000;
    cfg.minIns = 1;
    cfg.maxIns = 1;
    cfg.delayPerExec = 0;
    return cfg;
}

void testSchedulerFcfs() {
    Config cfg = baseSchedulerConfig();
    cfg.scheduler = "fcfs";
    cfg.quantumCycles = 1;

    SchedulerCore core(cfg, 1);
    std::string error;

    CHECK(core.addProcess("A", program({
        add("x", "x", "1"),
        add("x", "x", "1")
    }), error));
    CHECK(core.addProcess("B", program({
        add("y", "y", "1"),
        add("y", "y", "1")
    }), error));

    core.tickOnce();
    CHECK(core.getProcessView("A").process.state == ProcessState::RUNNING);
    CHECK(core.getProcessView("B").process.state == ProcessState::READY);

    core.tickOnce();
    CHECK(core.getProcessView("A").process.state == ProcessState::FINISHED);
    CHECK(core.getProcessView("B").process.state == ProcessState::READY);

    core.tickOnce();
    CHECK(core.getProcessView("B").process.state == ProcessState::RUNNING);
    CHECK_EQ(core.getProcessView("B").process.currentLine, static_cast<uint64_t>(2));
}

void testSchedulerRoundRobin() {
    Config cfg = baseSchedulerConfig();
    cfg.scheduler = "rr";
    cfg.quantumCycles = 2;

    SchedulerCore core(cfg, 2);
    std::string error;

    CHECK(core.addProcess("A", program({
        add("x", "x", "1"), add("x", "x", "1"), add("x", "x", "1"),
        add("x", "x", "1"), add("x", "x", "1")
    }), error));
    CHECK(core.addProcess("B", program({
        add("y", "y", "1"), add("y", "y", "1"), add("y", "y", "1"),
        add("y", "y", "1"), add("y", "y", "1")
    }), error));

    core.tickOnce();
    core.tickOnce();
    // A used its 2-cycle quantum and was preempted.
    CHECK(core.getProcessView("A").process.state == ProcessState::READY);
    CHECK_EQ(core.getProcessView("A").process.currentLine, static_cast<uint64_t>(3));

    core.tickOnce();
    core.tickOnce();
    // B used its quantum too.
    CHECK(core.getProcessView("B").process.state == ProcessState::READY);
    CHECK_EQ(core.getProcessView("A").process.currentLine, static_cast<uint64_t>(3));
    CHECK_EQ(core.getProcessView("B").process.currentLine, static_cast<uint64_t>(3));

    core.tickOnce();
    // A runs again before B.
    CHECK_EQ(core.getProcessView("A").process.currentLine, static_cast<uint64_t>(4));
    CHECK_EQ(core.getProcessView("B").process.currentLine, static_cast<uint64_t>(3));
}

void testBatchGeneration() {
    Config cfg = baseSchedulerConfig();
    cfg.numCpu = 2;
    cfg.scheduler = "rr";
    cfg.quantumCycles = 5;
    cfg.batchProcessFreq = 2;
    cfg.minIns = 5;
    cfg.maxIns = 5;

    SchedulerCore core(cfg, 3);
    core.startBatch();

    core.tickOnce();
    CHECK(!core.hasName("p01"));

    core.tickOnce();
    CHECK(core.hasName("p01"));

    core.tickOnce();
    CHECK(!core.hasName("p02"));

    core.tickOnce();
    CHECK(core.hasName("p02"));

    core.stopBatch();
    for (int i = 0; i < 10; ++i) core.tickOnce();
    CHECK(!core.hasName("p03"));

    std::string error;
    CHECK(core.createProcess("alpha", error));
    CHECK(!core.createProcess("alpha", error));
    CHECK(!core.createProcess("", error));
    CHECK(core.hasName("alpha"));
    CHECK(core.isActive("alpha"));
}

void testBatchSkipsExistingNames() {
    Config cfg = baseSchedulerConfig();
    cfg.batchProcessFreq = 1;
    cfg.minIns = 2;
    cfg.maxIns = 2;

    SchedulerCore core(cfg, 4);
    std::string error;
    CHECK(core.createProcess("p01", error));

    core.startBatch();
    core.tickOnce();
    CHECK(core.hasName("p02"));
    CHECK(!core.hasName("p01x"));
}

void testDelay() {
    Config cfg = baseSchedulerConfig();
    cfg.delayPerExec = 2;

    SchedulerCore core(cfg, 5);
    std::string error;
    CHECK(core.addProcess("slow", program({
        add("x", "x", "1"),
        add("x", "x", "1")
    }), error));

    core.tickOnce();
    CHECK_EQ(core.getProcessView("slow").process.currentLine, static_cast<uint64_t>(2));
    CHECK_EQ(core.snapshot().coresUsed, 1);

    core.tickOnce();
    CHECK_EQ(core.getProcessView("slow").process.currentLine, static_cast<uint64_t>(2));
    CHECK_EQ(core.snapshot().coresUsed, 1);

    core.tickOnce();
    CHECK_EQ(core.getProcessView("slow").process.currentLine, static_cast<uint64_t>(2));

    core.tickOnce();
    CHECK(core.getProcessView("slow").process.state == ProcessState::FINISHED);
}

void testSchedulerSleep() {
    Config cfg = baseSchedulerConfig();
    cfg.scheduler = "fcfs";

    SchedulerCore core(cfg, 6);
    std::string error;
    CHECK(core.addProcess("sleeper", program({
        sleepFor("2"),
        add("x", "x", "1")
    }), error));

    core.tickOnce();
    CHECK(core.getProcessView("sleeper").process.state == ProcessState::WAITING);
    CHECK_EQ(core.snapshot().coresUsed, 0);

    core.tickOnce();
    CHECK(core.getProcessView("sleeper").process.state == ProcessState::WAITING);
    CHECK_EQ(core.snapshot().coresUsed, 0);

    core.tickOnce();
    // Wakes, is dispatched, and executes the final instruction in the same tick.
    CHECK(core.getProcessView("sleeper").process.state == ProcessState::FINISHED);
}

void testSnapshot() {
    Config cfg = baseSchedulerConfig();
    cfg.numCpu = 2;

    SchedulerCore core(cfg, 7);
    std::string error;
    CHECK(core.addProcess("one", program({
        add("x", "x", "1"), add("x", "x", "1"),
        add("x", "x", "1"), add("x", "x", "1")
    }), error));

    core.tickOnce();
    const SystemSnapshot first = core.snapshot();
    CHECK_EQ(first.numCpu, 2);
    CHECK_EQ(first.coresUsed, 1);
    CHECK_EQ(first.coresAvailable, 1);
    CHECK_EQ(first.cpuUtilization, 50);
    CHECK_EQ(first.active.size(), static_cast<size_t>(1));
    CHECK_EQ(first.finished.size(), static_cast<size_t>(0));

    core.tickOnce();
    core.tickOnce();
    core.tickOnce();
    const SystemSnapshot done = core.snapshot();
    CHECK_EQ(done.coresUsed, 0);
    CHECK_EQ(done.active.size(), static_cast<size_t>(0));
    CHECK_EQ(done.finished.size(), static_cast<size_t>(1));
    CHECK_EQ(done.finished[0].name, std::string("one"));
}

bool containsLine(const std::vector<std::string>& lines, const std::string& needle) {
    return std::any_of(lines.begin(), lines.end(), [&needle](const std::string& line) {
        return line.find(needle) != std::string::npos;
    });
}

void testProcessViewFormatting() {
    ProcessSnapshot running;
    running.id = 3;
    running.name = "alpha";
    running.state = ProcessState::RUNNING;
    running.currentLine = 7;
    running.linesOfCode = 42;
    running.logs = {"(10/24/2026 02:06:38PM) Core:0 \"Hello world from alpha!\""};

    const std::vector<std::string> runningLines = ConsoleUI::formatProcessView(running);
    CHECK(containsLine(runningLines, "Process name: alpha"));
    CHECK(containsLine(runningLines, "Id: 3"));
    CHECK(containsLine(runningLines, "Logs:"));
    CHECK(containsLine(runningLines, "Hello world from alpha!"));
    CHECK(containsLine(runningLines, "Current instruction line: 7"));
    CHECK(containsLine(runningLines, "Lines of code: 42"));
    CHECK(!containsLine(runningLines, "Finished!"));

    ProcessSnapshot finished = running;
    finished.state = ProcessState::FINISHED;
    finished.currentLine = finished.linesOfCode;

    const std::vector<std::string> finishedLines = ConsoleUI::formatProcessView(finished);
    CHECK(containsLine(finishedLines, "Process name: alpha"));
    CHECK(containsLine(finishedLines, "Finished!"));
    CHECK(!containsLine(finishedLines, "Current instruction line"));
    CHECK(!containsLine(finishedLines, "Lines of code"));

    ProcessSnapshot empty;
    empty.name = "quiet";
    const std::vector<std::string> emptyLines = ConsoleUI::formatProcessView(empty);
    CHECK(containsLine(emptyLines, "(no output yet)"));
}

void testSystemListFormatting() {
    SystemSnapshot snapshot;
    snapshot.numCpu = 2;
    snapshot.coresUsed = 1;
    snapshot.coresAvailable = 1;
    snapshot.cpuUtilization = 50;

    ActiveProcessInfo active;
    active.id = 1;
    active.name = "alpha";
    active.state = ProcessState::RUNNING;
    active.core = 0;
    active.currentLine = 3;
    active.linesOfCode = 10;
    snapshot.active.push_back(active);

    FinishedProcessInfo done;
    done.id = 2;
    done.name = "beta";
    done.linesOfCode = 8;
    snapshot.finished.push_back(done);

    const std::vector<std::string> lines = ConsoleUI::formatSystemList(snapshot);
    CHECK(containsLine(lines, "CPU utilization: 50%"));
    CHECK(containsLine(lines, "Cores used: 1"));
    CHECK(containsLine(lines, "Cores available: 1"));
    CHECK(containsLine(lines, "Running processes:"));
    CHECK(containsLine(lines, "Finished processes:"));
    CHECK(containsLine(lines, "alpha"));
    CHECK(containsLine(lines, "Core: 0"));
    CHECK(containsLine(lines, "3 / 10"));
    CHECK(containsLine(lines, "beta"));
    CHECK(containsLine(lines, "8 / 8"));

    const std::vector<std::string> emptyLines =
        ConsoleUI::formatSystemList(SystemSnapshot{});
    CHECK(containsLine(emptyLines, "(none)"));
    CHECK_EQ(emptyLines.size(), static_cast<size_t>(9));
}

} // namespace

int main() {
    testConfigParsing();
    testArithmeticAndPrint();
    testClamping();
    testPrintVariants();
    testForLoops();
    testTemplateRepeat();
    testLogCap();
    testProcessSleep();
    testGeneratedProgram();
    testSchedulerFcfs();
    testSchedulerRoundRobin();
    testBatchGeneration();
    testBatchSkipsExistingNames();
    testDelay();
    testSchedulerSleep();
    testSnapshot();
    testProcessViewFormatting();
    testSystemListFormatting();

    std::cout << "Checks run: " << g_checks << ", failures: " << g_failures << '\n';
    if (g_failures != 0) {
        std::cout << "TESTS FAILED\n";
        return 1;
    }

    std::cout << "ALL TESTS PASSED\n";
    return 0;
}
