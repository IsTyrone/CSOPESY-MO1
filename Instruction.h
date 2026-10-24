#pragma once

#include <cstdint>
#include <random>
#include <string>
#include <vector>

enum class Op {
    PRINT,
    DECLARE,
    ADD,
    SUBTRACT,
    SLEEP,
    FOR
};

struct Instruction {
    Op op = Op::PRINT;

    // Operand meanings depend on the opcode:
    //   PRINT:    a = literal prefix (may be empty), b = optional variable name
    //   DECLARE:  a = destination variable, b = value/operand
    //   ADD/SUB:  a = destination variable, b = left operand, c = right operand
    //   SLEEP:    a = tick count operand
    //   FOR:      repeats = number of body executions, body = nested instructions
    std::string a;
    std::string b;
    std::string c;

    uint64_t repeats = 1;
    std::vector<Instruction> body;
};

struct Program {
    std::vector<Instruction> code;
    uint64_t totalLines = 0;
};

// Builds a compact instruction template that is repeated until totalLines is
// reached. This keeps memory bounded even for the spec's 2^32 upper bound.
Program generateProgram(uint64_t minIns, uint64_t maxIns, std::mt19937_64& rng);
