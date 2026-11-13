#pragma once

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "flux/AST.h"
#include "flux/Lexer.h"

namespace flux {

// Performs pre-execution checks that depend on program structure rather than
// grammar alone. The analyser reports every recoverable semantic error in one run.
class SemanticAnalyzer {
public:
    void analyze(const Program& program);
    const std::vector<Diagnostic>& diagnostics() const;

private:
    struct Scope {
        std::unordered_set<std::string> variables;
        std::unordered_set<std::string> constants;
    };

    struct FunctionContext {
        std::string name;
        std::size_t scopeIndex;
    };

    void analyzeStatement(const Stmt& statement);
    void analyzeBlock(const BlockStmt& block);
    void analyzeExpression(const Expr& expression);
    void analyzeCondition(const Expr& expression);
    void declareTarget(const Expr& target, bool constant);
    std::string targetName(const Expr& target) const;
    bool isKnownConstant(const std::string& name) const;
    bool existsOutsideFunction(const std::string& name) const;
    void report(const Token& token, std::string message);

    std::vector<Diagnostic> diagnostics_;
    std::vector<Scope> scopes_;
    std::vector<FunctionContext> functions_;
};

}  // namespace flux
