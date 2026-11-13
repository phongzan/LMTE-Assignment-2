#include "flux/SemanticAnalyzer.h"

#include <memory>
#include <string>
#include <utility>

namespace flux {

namespace {

// Dynamic Flux expressions cannot always be typed before execution. This
// helper rejects only expressions that are certainly non-Boolean.
bool isDefinitelyNonBoolean(const Expr& expression) {
    if (const auto* literal = dynamic_cast<const LiteralExpr*>(&expression)) {
        return !literal->value.isBool();
    }
    return dynamic_cast<const ArrayExpr*>(&expression) != nullptr;
}

}  // namespace

void SemanticAnalyzer::analyze(const Program& program) {
    diagnostics_.clear();
    scopes_.clear();
    functions_.clear();
    scopes_.push_back({});

    for (const auto& statement : program.statements) {
        analyzeStatement(*statement);
    }
}

const std::vector<Diagnostic>& SemanticAnalyzer::diagnostics() const {
    return diagnostics_;
}

void SemanticAnalyzer::analyzeStatement(const Stmt& statement) {
    if (const auto* expression = dynamic_cast<const ExpressionStmt*>(&statement)) {
        analyzeExpression(*expression->expression);
        return;
    }

    if (const auto* assignment = dynamic_cast<const AssignmentStmt*>(&statement)) {
        analyzeExpression(*assignment->value);
        analyzeExpression(*assignment->target);
        const std::string name = targetName(*assignment->target);
        if (name.empty()) return;

        if (assignment->operation.type == TokenType::FatArrow) {
            declareTarget(*assignment->target, true);
            return;
        }

        if (isKnownConstant(name)) {
            report(assignment->operation, "cannot assign to constant " + name);
            return;
        }

        if (assignment->outer) {
            if (functions_.empty() || !existsOutsideFunction(name)) {
                const std::string functionName = functions_.empty()
                                                     ? "the current function"
                                                     : functions_.back().name;
                report(assignment->operation,
                       "no variable '" + name + "' is assigned outside " + functionName);
            }
            return;
        }

        // Assigning a simple name creates it in the current scope. Indexed
        // assignments mutate an existing array and therefore declare nothing.
        if (dynamic_cast<const VariableExpr*>(assignment->target.get()) != nullptr) {
            scopes_.back().variables.insert(name);
        }
        return;
    }

    if (const auto* show = dynamic_cast<const ShowStmt*>(&statement)) {
        analyzeExpression(*show->expression);
        return;
    }

    if (const auto* give = dynamic_cast<const GiveStmt*>(&statement)) {
        analyzeExpression(*give->expression);
        if (functions_.empty()) {
            report(give->token, "'give' outside a function");
        }
        return;
    }

    if (const auto* rewind = dynamic_cast<const RewindStmt*>(&statement)) {
        if (isKnownConstant(rewind->name.lexeme)) {
            report(rewind->token, "cannot rewind constant " + rewind->name.lexeme);
        }
        return;
    }

    if (const auto* when = dynamic_cast<const WhenStmt*>(&statement)) {
        for (const auto& branch : when->branches) {
            analyzeCondition(*branch.condition);
            analyzeBlock(*branch.body);
        }
        if (when->otherwiseBranch != nullptr) analyzeBlock(*when->otherwiseBranch);
        return;
    }

    if (const auto* repeat = dynamic_cast<const RepeatStmt*>(&statement)) {
        analyzeExpression(*repeat->count);
        scopes_.push_back({});
        scopes_.back().variables.insert(repeat->variable.lexeme);
        analyzeBlock(*repeat->body);
        scopes_.pop_back();
        return;
    }

    if (const auto* until = dynamic_cast<const UntilStmt*>(&statement)) {
        analyzeCondition(*until->condition);
        analyzeBlock(*until->body);
        return;
    }

    if (const auto* each = dynamic_cast<const EachStmt*>(&statement)) {
        analyzeExpression(*each->collection);
        scopes_.push_back({});
        scopes_.back().variables.insert(each->variable.lexeme);
        analyzeBlock(*each->body);
        scopes_.pop_back();
        return;
    }

    if (const auto* function = dynamic_cast<const FunctionStmt*>(&statement)) {
        scopes_.back().variables.insert(function->name.lexeme);
        scopes_.push_back({});
        functions_.push_back({function->name.lexeme, scopes_.size() - 1});

        std::unordered_set<std::string> parameters;
        for (const Token& parameter : function->parameters) {
            if (!parameters.insert(parameter.lexeme).second) {
                report(parameter, "duplicate parameter '" + parameter.lexeme +
                                      "' in " + function->name.lexeme);
            }
            scopes_.back().variables.insert(parameter.lexeme);
        }

        analyzeBlock(*function->body);
        functions_.pop_back();
        scopes_.pop_back();
    }
}

void SemanticAnalyzer::analyzeBlock(const BlockStmt& block) {
    // Flux control-flow blocks update their enclosing scope directly, so a block
    // does not create a new semantic scope by itself.
    for (const auto& statement : block.statements) {
        analyzeStatement(*statement);
    }
}

void SemanticAnalyzer::analyzeExpression(const Expr& expression) {
    if (const auto* array = dynamic_cast<const ArrayExpr*>(&expression)) {
        for (const auto& element : array->elements) analyzeExpression(*element);
    } else if (const auto* unary = dynamic_cast<const UnaryExpr*>(&expression)) {
        analyzeExpression(*unary->right);
    } else if (const auto* binary = dynamic_cast<const BinaryExpr*>(&expression)) {
        analyzeExpression(*binary->left);
        analyzeExpression(*binary->right);
    } else if (const auto* call = dynamic_cast<const CallExpr*>(&expression)) {
        analyzeExpression(*call->callee);
        for (const auto& argument : call->arguments) analyzeExpression(*argument);
    } else if (const auto* index = dynamic_cast<const IndexExpr*>(&expression)) {
        analyzeExpression(*index->object);
        analyzeExpression(*index->index);
    } else if (const auto* history = dynamic_cast<const HistoryExpr*>(&expression)) {
        analyzeExpression(*history->object);
    } else if (const auto* pipeline = dynamic_cast<const PipelineExpr*>(&expression)) {
        analyzeExpression(*pipeline->input);
        analyzeExpression(*pipeline->step);
    }
}

void SemanticAnalyzer::analyzeCondition(const Expr& expression) {
    analyzeExpression(expression);
    if (isDefinitelyNonBoolean(expression)) {
        report(expression.token, "condition must be true or false");
    }
}

void SemanticAnalyzer::declareTarget(const Expr& target, bool constant) {
    const std::string name = targetName(target);
    if (name.empty()) return;

    if (isKnownConstant(name) || scopes_.back().variables.count(name) != 0) {
        report(target.token, "name '" + name + "' is already defined");
        return;
    }

    if (constant) scopes_.back().constants.insert(name);
    else scopes_.back().variables.insert(name);
}

std::string SemanticAnalyzer::targetName(const Expr& target) const {
    if (const auto* variable = dynamic_cast<const VariableExpr*>(&target)) {
        return variable->name.lexeme;
    }
    if (const auto* index = dynamic_cast<const IndexExpr*>(&target)) {
        return targetName(*index->object);
    }
    return {};
}

bool SemanticAnalyzer::isKnownConstant(const std::string& name) const {
    for (auto scope = scopes_.rbegin(); scope != scopes_.rend(); ++scope) {
        if (scope->constants.count(name) != 0) return true;
        if (scope->variables.count(name) != 0) return false;
    }
    return false;
}

bool SemanticAnalyzer::existsOutsideFunction(const std::string& name) const {
    if (functions_.empty()) return false;
    const std::size_t functionScope = functions_.back().scopeIndex;
    for (std::size_t index = functionScope; index-- > 0;) {
        if (scopes_[index].variables.count(name) != 0 ||
            scopes_[index].constants.count(name) != 0) {
            return true;
        }
    }
    return false;
}

void SemanticAnalyzer::report(const Token& token, std::string message) {
    diagnostics_.push_back({"Semantic", token.line, token.column, std::move(message)});
}

}  // namespace flux
