#pragma once

#include <cstdint>
#include <iostream>
#include <memory>
#include <vector>

#include "flux/CodeGen.h"
#include "flux/Environment.h"
#include "flux/RuntimeError.h"
#include "flux/Value.h"

namespace flux {

// Virtual Machine: executes bytecode
class VM {
  public:
    VM(std::istream& input, std::ostream& output, const std::vector<Instruction>& code,
        const ConstantPool& constants, bool showBytecode = false);

    // Execute the bytecode
    void execute();

    // Get memory statistics
    MemoryStats memoryStats() const { return Value::memoryStats(); }

  private:
    // Bytecode and constants
    const std::vector<Instruction>& code_;
    const ConstantPool& constants_;
    bool showBytecode_;

    // Execution state
    uint32_t pc_;  // Program counter
    std::vector<Value> stack_;
    std::unique_ptr<Environment> globalEnv_;
    std::vector<Environment*> envStack_;  // Stack of active environments for scope

    // Input/output
    std::istream& input_;
    std::ostream& output_;

    // Function call tracking
    int callDepth_;
    static const int MAX_CALL_DEPTH = 1000;
    static const int64_t MAX_ITERATIONS = 1000000;

    // Execution helpers
    void executeInstruction(const Instruction& instr);
    Value pop();
    void push(const Value& val);
    Value peek(int offset = 0) const;

    // Opcode handlers
    void handlePushNum(double num);
    void handlePushStr(uint32_t strIdx);
    void handlePushTrue();
    void handlePushFalse();
    void handlePushArray(uint32_t count);
    void handleLoad(uint32_t nameIdx);
    void handleStore(uint32_t nameIdx);
    void handleStoreOuter(uint32_t nameIdx);
    void handleStoreIndex(uint32_t nameIdx, uint32_t depth);
    void handleAdd();
    void handleSub();
    void handleMul();
    void handleDiv();
    void handleMod();
    void handleNeg();
    void handleNot();
    void handleEq();
    void handleNe();
    void handleLt();
    void handleLe();
    void handleGt();
    void handleGe();
    void handleAnd();
    void handleOr();
    void handleIndex();
    void handleLoadHist(uint32_t nameIdx, uint32_t offset);
    void handleHistory(uint32_t nameIdx);
    void handleRewind(uint32_t nameIdx);
    void handleJump(uint32_t addr);
    void handleJumpIfFalse(uint32_t addr);
    void handleCall(uint32_t nameIdx, uint32_t argc);
    void handleShow();
    void handleAsk(uint32_t promptIdx);
    void handleIterEach(uint32_t varIdx, uint32_t collectionIdx, uint32_t endAddr);
    void handleMakeArray(uint32_t count);
    void handleGive();

    // Utility methods
    std::string getNameFromIndex(uint32_t idx) const { return constants_.names[idx]; }
    std::string getStringFromIndex(uint32_t idx) const { return constants_.strings[idx]; }
    double getNumberFromIndex(uint32_t idx) const { return constants_.numbers[idx]; }

    // Built-in functions
    Value callBuiltin(const std::string& name, const std::vector<Value>& args);
    Value builtinLen(const std::vector<Value>& args);
    Value builtinPush(const std::vector<Value>& args);
    Value builtinHistory(const std::vector<Value>& args);

    // Assertion for debugging
    void assertStackSize(size_t expected);
};

}  // namespace flux
