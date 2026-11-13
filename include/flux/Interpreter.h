#pragma once

#include <cstddef>
#include <istream>
#include <memory>
#include <ostream>
#include <string>
#include <unordered_map>
#include <vector>

#include "flux/AST.h"
#include "flux/Environment.h"

namespace flux {

// Executes a validated Flux AST directly. The interpreter owns the global
// environment and uses child environments for function calls and loop variables.
class Interpreter {
public:
    Interpreter(std::istream& input, std::ostream& output);

    void interpret(const Program& program);
    std::shared_ptr<Environment> globals() const;

private:
    struct Function {
        const FunctionStmt* declaration;
        std::shared_ptr<Environment> closure;
    };

    class ReturnSignal;

    void execute(const Stmt& statement);
    void executeBlock(const BlockStmt& block);
    Value evaluate(const Expr& expression);
    Value evaluateCall(const CallExpr& call, std::vector<Value> prefix = {});
    Value callNamed(const std::string& name, std::vector<Value> arguments,
                    const Token& token);
    Value callFunction(const Function& function, const std::vector<Value>& arguments,
                       const Token& token);
    Value callBuiltin(const std::string& name, const std::vector<Value>& arguments,
                      const Token& token);

    void assignTarget(const AssignmentStmt& assignment, const Value& value);
    void writeIndexed(Value& root, const std::vector<std::size_t>& indices,
                      const Value& value);
    void collectTarget(const Expr& target, std::string& name,
                       std::vector<std::size_t>& indices);
    bool condition(const Expr& expression);
    static std::size_t arrayIndex(const Value& value, std::size_t size);

    std::istream& input_;
    std::ostream& output_;
    std::shared_ptr<Environment> globals_;
    std::shared_ptr<Environment> current_;
    std::unordered_map<std::string, Function> functions_;
    std::size_t callDepth_ = 0;
    bool updateNearest_ = false;
};

}  // namespace flux
