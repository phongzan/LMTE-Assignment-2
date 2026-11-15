#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "flux/AST.h"

namespace flux {

// Bytecode instruction format: opcode (8-bit) + operand (24-bit, or two 12-bit operands, or immediate)
struct Instruction {
    enum OpCode : uint8_t {
        HALT = 0,
        PUSH_NUM = 1,
        PUSH_STR = 2,
        PUSH_TRUE = 3,
        PUSH_FALSE = 4,
        PUSH_ARRAY = 5,
        LOAD = 6,
        STORE = 7,
        STORE_OUTER = 8,
        STORE_INDEX = 9,
        ADD = 10,
        SUB = 11,
        MUL = 12,
        DIV = 13,
        MOD = 14,
        NEG = 15,
        NOT = 16,
        EQ = 17,
        NE = 18,
        LT = 19,
        LE = 20,
        GT = 21,
        GE = 22,
        AND = 23,
        OR = 24,
        INDEX = 25,
        LOAD_HIST = 26,
        HISTORY = 27,
        REWIND = 28,
        POP = 29,
        DUP = 30,
        JUMP = 31,
        JUMP_IF_FALSE = 32,
        CALL = 33,
        RETURN = 34,
        SHOW = 35,
        ASK = 36,
        ITER_EACH = 37,
        DEFINE_FUNC = 38,
        GIVE = 39,
        PIPELINE = 40,
        MAKE_ARRAY = 41,
    };

    OpCode opcode;
    uint32_t operand1;
    uint32_t operand2;
    double numOperand;

    Instruction(OpCode op = HALT, uint32_t o1 = 0, uint32_t o2 = 0, double numOp = 0.0)
        : opcode(op), operand1(o1), operand2(o2), numOperand(numOp) {}
};

// Constant pool: immutable values shared across the program
struct ConstantPool {
    std::vector<std::string> strings;  // String literals
    std::vector<std::string> names;    // Variable/function names
    std::vector<double> numbers;       // Number literals

    uint32_t addString(const std::string& s);
    uint32_t addName(const std::string& name);
    uint32_t addNumber(double num);
};

// Function information stored during code generation
struct FunctionInfo {
    std::string name;
    std::vector<std::string> parameters;
    uint32_t codeStart;
    uint32_t codeEnd;
    bool isBuiltin;
};

// Code generator: AST → bytecode
class CodeGen {
  public:
    CodeGen();

    // Generate bytecode from an AST program
    std::vector<Instruction> generate(const Program& program);

    // Get the constant pool
    const ConstantPool& constants() const { return constantPool_; }

    // Get function information
    const std::vector<FunctionInfo>& functions() const { return functions_; }

  private:
    std::vector<Instruction> code_;
    ConstantPool constantPool_;
    std::vector<FunctionInfo> functions_;
    std::unordered_map<std::string, uint32_t> nameToIndex_;
    std::unordered_map<std::string, uint32_t> stringToIndex_;
    std::unordered_map<double, uint32_t> numberToIndex_;
    int scopeDepth_;

    // Emit instructions
    void emit(const Instruction& instr);
    void emit(Instruction::OpCode op, uint32_t o1 = 0, uint32_t o2 = 0, double numOp = 0.0);
    uint32_t currentAddress() const;
    void patchJump(uint32_t addr, uint32_t target);

    // Code generation for AST nodes
    void generateStmt(const Stmt& stmt);
    void generateExpr(const Expr& expr);

    // Type-specific generators
    void generateBinaryExpr(const BinaryExpr& expr);
    void generateUnaryExpr(const UnaryExpr& expr);
    void generateCallExpr(const CallExpr& expr);
    void generateIndexExpr(const IndexExpr& expr);
    void generateHistoryExpr(const HistoryExpr& expr);
    void generatePipelineExpr(const PipelineExpr& expr);
    void generateArrayExpr(const ArrayExpr& expr);
    void generateVariableExpr(const VariableExpr& expr);
    void generateLiteralExpr(const LiteralExpr& expr);
    void generateAskExpr(const AskExpr& expr);

    void generateExpressionStmt(const ExpressionStmt& stmt);
    void generateAssignmentStmt(const AssignmentStmt& stmt);
    void generateShowStmt(const ShowStmt& stmt);
    void generateGiveStmt(const GiveStmt& stmt);
    void generateRewindStmt(const RewindStmt& stmt);
    void generateBlockStmt(const BlockStmt& stmt);
    void generateWhenStmt(const WhenStmt& stmt);
    void generateRepeatStmt(const RepeatStmt& stmt);
    void generateUntilStmt(const UntilStmt& stmt);
    void generateEachStmt(const EachStmt& stmt);
    void generateFunctionStmt(const FunctionStmt& stmt);

    // Helper methods
    uint32_t getNameIndex(const std::string& name);
    uint32_t getStringIndex(const std::string& str);

    // Optimization passes
    void optimizeConstantFolding();
    void optimizeDeadCode();
    bool isConstantExpr(const Expr& expr);
    bool evaluateConstantExpr(const Expr& expr, Value& result);
};

}  // namespace flux
