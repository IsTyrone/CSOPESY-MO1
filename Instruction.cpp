#include "Instruction.h"

#include <algorithm>
#include <array>

namespace {

constexpr uint64_t kTemplateLimit = 128;

std::string pickVar(std::mt19937_64& rng) {
    static const std::array<const char*, 3> names = {"x", "y", "z"};
    std::uniform_int_distribution<int> pick(0, static_cast<int>(names.size()) - 1);
    return names[pick(rng)];
}

std::string pickOperand(std::mt19937_64& rng) {
    std::uniform_int_distribution<int> pick(0, 1);
    if (pick(rng) == 0) return pickVar(rng);

    std::uniform_int_distribution<int> value(0, 100);
    return std::to_string(value(rng));
}

Instruction makeDeclare(std::mt19937_64& rng, const std::string& var) {
    Instruction ins;
    ins.op = Op::DECLARE;
    ins.a = var;
    std::uniform_int_distribution<int> value(0, 100);
    ins.b = std::to_string(value(rng));
    return ins;
}

Instruction makePrintDefault() {
    Instruction ins;
    ins.op = Op::PRINT;
    return ins;
}

Instruction makeSleep(std::mt19937_64& rng) {
    Instruction ins;
    ins.op = Op::SLEEP;
    std::uniform_int_distribution<int> ticks(1, 3);
    ins.a = std::to_string(ticks(rng));
    return ins;
}

Instruction makeArithmetic(std::mt19937_64& rng, bool add) {
    Instruction ins;
    ins.op = add ? Op::ADD : Op::SUBTRACT;
    ins.a = pickVar(rng);
    ins.b = pickOperand(rng);
    ins.c = pickOperand(rng);
    return ins;
}

} // namespace

Program generateProgram(uint64_t minIns, uint64_t maxIns, std::mt19937_64& rng) {
    if (minIns == 0) minIns = 1;
    if (maxIns < minIns) maxIns = minIns;

    std::uniform_int_distribution<uint64_t> count(minIns, maxIns);
    const uint64_t totalLines = count(rng);

    const uint64_t templateLen = std::min<uint64_t>(totalLines, kTemplateLimit);

    Program program;
    program.totalLines = totalLines;
    program.code.reserve(static_cast<size_t>(templateLen));

    if (templateLen == 1) {
        program.code.push_back(makePrintDefault());
        return program;
    }

    for (uint64_t i = 0; i < templateLen; ++i) {
        if (i == 0) {
            program.code.push_back(makeDeclare(rng, "x"));
        } else if (i == 1) {
            program.code.push_back(makeDeclare(rng, "y"));
        } else if (i == 2 && templateLen > 2) {
            program.code.push_back(makeDeclare(rng, "z"));
        } else if (i % 23 == 7) {
            program.code.push_back(makePrintDefault());
        } else if (i % 31 == 13) {
            program.code.push_back(makeSleep(rng));
        } else if (i % 11 == 0) {
            program.code.push_back(makeDeclare(rng, pickVar(rng)));
        } else if (i % 2 == 0) {
            program.code.push_back(makeArithmetic(rng, true));
        } else {
            program.code.push_back(makeArithmetic(rng, false));
        }
    }

    const bool hasPrint = std::any_of(
        program.code.begin(), program.code.end(),
        [](const Instruction& ins) { return ins.op == Op::PRINT; });
    if (!hasPrint) {
        program.code.back() = makePrintDefault();
    }

    return program;
}
