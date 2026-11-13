#include "flux/Interpreter.h"

#include <cmath>
#include <cstdlib>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "flux/Operators.h"
#include "flux/RuntimeError.h"

namespace flux {

class Interpreter::ReturnSignal final : public std::exception {
public:
    explicit ReturnSignal(Value value) : value(std::move(value)) {}
    Value value;
};

Interpreter::Interpreter(std::istream& input, std::ostream& output)
    : input_(input), output_(output), globals_(std::make_shared<Environment>()),
      current_(globals_) {}

void Interpreter::interpret(const Program& program) {
    for (const auto& statement : program.statements) {
        try {
            execute(*statement);
        } catch (const RuntimeError& error) {
            throw RuntimeError("line " + std::to_string(statement->token.line) + ": " +
                               error.what());
        }
    }
}

std::shared_ptr<Environment> Interpreter::globals() const { return globals_; }

void Interpreter::execute(const Stmt& statement) {
    if (const auto* expression = dynamic_cast<const ExpressionStmt*>(&statement)) {
        evaluate(*expression->expression);
        return;
    }
    if (const auto* assignment = dynamic_cast<const AssignmentStmt*>(&statement)) {
        const Value value = evaluate(*assignment->value);
        assignTarget(*assignment, value);
        return;
    }
    if (const auto* show = dynamic_cast<const ShowStmt*>(&statement)) {
        output_ << evaluate(*show->expression).toString() << '\n';
        return;
    }
    if (const auto* give = dynamic_cast<const GiveStmt*>(&statement)) {
        throw ReturnSignal(evaluate(*give->expression));
    }
    if (const auto* rewind = dynamic_cast<const RewindStmt*>(&statement)) {
        current_->rewind(rewind->name.lexeme);
        return;
    }
    if (const auto* when = dynamic_cast<const WhenStmt*>(&statement)) {
        for (const auto& branch : when->branches) {
            if (condition(*branch.condition)) {
                executeBlock(*branch.body);
                return;
            }
        }
        if (when->otherwiseBranch != nullptr) executeBlock(*when->otherwiseBranch);
        return;
    }
    if (const auto* repeat = dynamic_cast<const RepeatStmt*>(&statement)) {
        const double countValue = evaluate(*repeat->count).asNumber();
        if (countValue < 0 || std::floor(countValue) != countValue) {
            throw RuntimeError("repeat count must be a non-negative integer");
        }
        auto previous = current_;
        current_ = std::make_shared<Environment>(previous);
        current_->defineVariable(repeat->variable.lexeme, Value(0.0));
        const bool oldUpdate = updateNearest_;
        updateNearest_ = true;
        for (std::size_t index = 0; index < static_cast<std::size_t>(countValue); ++index) {
            if (index != 0) current_->assign(repeat->variable.lexeme, Value(static_cast<double>(index)));
            auto loop = current_;
            current_ = std::make_shared<Environment>(loop);
            executeBlock(*repeat->body);
            current_ = loop;
        }
        updateNearest_ = oldUpdate;
        current_ = previous;
        return;
    }
    if (const auto* until = dynamic_cast<const UntilStmt*>(&statement)) {
        std::size_t iterations = 0;
        while (!condition(*until->condition)) {
            if (++iterations > 1000000) throw RuntimeError("loop iteration limit exceeded");
            executeBlock(*until->body);
        }
        return;
    }
    if (const auto* each = dynamic_cast<const EachStmt*>(&statement)) {
        const auto elements = evaluate(*each->collection).asArray();
        auto previous = current_;
        current_ = std::make_shared<Environment>(previous);
        if (!elements.empty()) current_->defineVariable(each->variable.lexeme, elements.front());
        const bool oldUpdate = updateNearest_;
        updateNearest_ = true;
        for (std::size_t index = 0; index < elements.size(); ++index) {
            if (index != 0) current_->assign(each->variable.lexeme, elements[index]);
            auto loop = current_;
            current_ = std::make_shared<Environment>(loop);
            executeBlock(*each->body);
            current_ = loop;
        }
        updateNearest_ = oldUpdate;
        current_ = previous;
        return;
    }
    if (const auto* function = dynamic_cast<const FunctionStmt*>(&statement)) {
        functions_.insert_or_assign(function->name.lexeme, Function{function, current_});
    }
}

void Interpreter::executeBlock(const BlockStmt& block) {
    for (const auto& statement : block.statements) execute(*statement);
}

Value Interpreter::evaluate(const Expr& expression) {
    if (const auto* literal = dynamic_cast<const LiteralExpr*>(&expression)) {
        return literal->value.deepCopy();
    }
    if (const auto* variable = dynamic_cast<const VariableExpr*>(&expression)) {
        return current_->get(variable->name.lexeme).deepCopy();
    }
    if (const auto* array = dynamic_cast<const ArrayExpr*>(&expression)) {
        std::vector<Value> elements;
        elements.reserve(array->elements.size());
        for (const auto& element : array->elements) elements.push_back(evaluate(*element));
        return Value::array(std::move(elements));
    }
    if (const auto* unary = dynamic_cast<const UnaryExpr*>(&expression)) {
        const Value right = evaluate(*unary->right);
        if (unary->operation.type == TokenType::Minus) return operators::negate(right);
        return operators::logicalNot(right);
    }
    if (const auto* binary = dynamic_cast<const BinaryExpr*>(&expression)) {
        if (binary->operation.type == TokenType::And) {
            const bool left = evaluate(*binary->left).asBool();
            return Value(left && evaluate(*binary->right).asBool());
        }
        if (binary->operation.type == TokenType::Or) {
            const bool left = evaluate(*binary->left).asBool();
            return Value(left || evaluate(*binary->right).asBool());
        }
        const Value left = evaluate(*binary->left);
        const Value right = evaluate(*binary->right);
        switch (binary->operation.type) {
            case TokenType::Plus: return operators::add(left, right);
            case TokenType::Minus: return operators::subtract(left, right);
            case TokenType::Star: return operators::multiply(left, right);
            case TokenType::Slash: return operators::divide(left, right);
            case TokenType::Percent: return operators::modulo(left, right);
            case TokenType::EqualEqual: return operators::equal(left, right);
            case TokenType::BangEqual: return Value(left != right);
            case TokenType::Less: return operators::less(left, right);
            case TokenType::LessEqual: return operators::lessEqual(left, right);
            case TokenType::Greater: return operators::greater(left, right);
            case TokenType::GreaterEqual: return operators::greaterEqual(left, right);
            default: break;
        }
    }
    if (const auto* call = dynamic_cast<const CallExpr*>(&expression)) {
        return evaluateCall(*call);
    }
    if (const auto* index = dynamic_cast<const IndexExpr*>(&expression)) {
        const Value object = evaluate(*index->object);
        const std::size_t position = arrayIndex(evaluate(*index->index), object.asArray().size());
        return object.asArray()[position].deepCopy();
    }
    if (const auto* history = dynamic_cast<const HistoryExpr*>(&expression)) {
        const auto* variable = dynamic_cast<const VariableExpr*>(history->object.get());
        if (variable == nullptr) throw RuntimeError("history operator needs a variable");
        return current_->previous(variable->name.lexeme, history->assignmentsAgo);
    }
    if (const auto* ask = dynamic_cast<const AskExpr*>(&expression)) {
        output_ << ask->prompt;
        std::string line;
        if (!std::getline(input_, line)) throw RuntimeError("input ended while reading 'ask'");
        char* end = nullptr;
        const double number = std::strtod(line.c_str(), &end);
        if (end != line.c_str() && *end == '\0') return Value(number);
        return Value(line);
    }
    if (const auto* pipeline = dynamic_cast<const PipelineExpr*>(&expression)) {
        Value input = evaluate(*pipeline->input);
        if (const auto* call = dynamic_cast<const CallExpr*>(pipeline->step.get())) {
            return evaluateCall(*call, {input});
        }
        if (const auto* variable = dynamic_cast<const VariableExpr*>(pipeline->step.get())) {
            if (variable->name.type == TokenType::Show || variable->name.lexeme == "show") {
                output_ << input.toString() << '\n';
                return Value();
            }
            return callNamed(variable->name.lexeme, {input}, variable->name);
        }
        throw RuntimeError("'then' needs a function on its right");
    }
    throw RuntimeError("unsupported expression");
}

Value Interpreter::evaluateCall(const CallExpr& call, std::vector<Value> prefix) {
    const auto* callee = dynamic_cast<const VariableExpr*>(call.callee.get());
    if (callee == nullptr) throw RuntimeError("call target must be a function name");

    // history() needs the variable name itself rather than only its current
    // value, so handle it before ordinary argument evaluation discards that name.
    if (callee->name.lexeme == "history") {
        if (!prefix.empty() || call.arguments.size() != 1) {
            throw RuntimeError("history expects 1 variable argument");
        }
        const auto* variable = dynamic_cast<const VariableExpr*>(call.arguments[0].get());
        if (variable == nullptr) throw RuntimeError("history() requires a variable name");
        return Value::array(current_->history(variable->name.lexeme));
    }

    for (const auto& argument : call.arguments) prefix.push_back(evaluate(*argument));
    return callNamed(callee->name.lexeme, std::move(prefix), callee->name);
}

Value Interpreter::callNamed(const std::string& name, std::vector<Value> arguments,
                             const Token& token) {
    if (name == "len" || name == "push") {
        return callBuiltin(name, arguments, token);
    }
    const auto iterator = functions_.find(name);
    if (iterator == functions_.end()) throw RuntimeError("undefined function '" + name + "'");
    return callFunction(iterator->second, arguments, token);
}

Value Interpreter::callFunction(const Function& function,
                                const std::vector<Value>& arguments,
                                const Token&) {
    const auto& parameters = function.declaration->parameters;
    if (arguments.size() != parameters.size()) {
        throw RuntimeError(function.declaration->name.lexeme + " expects " +
                           std::to_string(parameters.size()) + " argument(s), got " +
                           std::to_string(arguments.size()));
    }
    // Keep the language limit below the native C++ stack limit so runaway
    // recursion becomes a catchable Flux error rather than a process crash.
    if (++callDepth_ > 1000) {
        --callDepth_;
        throw RuntimeError("call depth limit (1,000) exceeded");
    }

    auto previous = current_;
    current_ = std::make_shared<Environment>(function.closure);
    for (std::size_t index = 0; index < parameters.size(); ++index) {
        current_->defineVariable(parameters[index].lexeme, arguments[index]);
    }

    Value result;
    try {
        executeBlock(*function.declaration->body);
    } catch (const ReturnSignal& signal) {
        result = signal.value.deepCopy();
    } catch (...) {
        current_ = previous;
        --callDepth_;
        throw;
    }
    current_ = previous;
    --callDepth_;
    return result;
}

Value Interpreter::callBuiltin(const std::string& name,
                               const std::vector<Value>& arguments,
                               const Token&) {
    if (name == "len") {
        if (arguments.size() != 1) throw RuntimeError("len expects 1 argument");
        if (arguments[0].isArray()) return Value(static_cast<double>(arguments[0].asArray().size()));
        if (arguments[0].isString()) return Value(static_cast<double>(arguments[0].asString().size()));
        throw RuntimeError("len needs an array or string");
    }
    if (name == "push") {
        if (arguments.size() != 2) throw RuntimeError("push expects 2 arguments");
        Value result = arguments[0].deepCopy();
        result.asMutableArray().push_back(arguments[1].deepCopy());
        return result;
    }
    throw RuntimeError("unknown builtin function '" + name + "'");
}

void Interpreter::assignTarget(const AssignmentStmt& assignment, const Value& value) {
    std::string name;
    std::vector<std::size_t> indices;
    collectTarget(*assignment.target, name, indices);
    if (name.empty()) throw RuntimeError("invalid assignment target");

    // Values are immutable snapshots until a mutable array accessor triggers
    // copy-on-write, so ordinary assignment can share the evaluated value.
    Value assigned = value;
    if (!indices.empty()) {
        assigned = current_->get(name).deepCopy();
        writeIndexed(assigned, indices, value);
    }

    if (assignment.operation.type == TokenType::FatArrow) {
        current_->defineConstant(name, assigned);
    } else if (assignment.outer) {
        current_->assign(name, assigned, true);
    } else if (updateNearest_) {
        current_->assignNearest(name, assigned);
    } else {
        current_->assign(name, assigned);
    }
}

void Interpreter::writeIndexed(Value& root, const std::vector<std::size_t>& indices,
                               const Value& value) {
    Value* current = &root;
    for (std::size_t depth = 0; depth < indices.size(); ++depth) {
        auto& elements = current->asMutableArray();
        const std::size_t index = indices[depth];
        if (index >= elements.size()) {
            throw RuntimeError("index " + std::to_string(index) + " out of range (size " +
                               std::to_string(elements.size()) + ")");
        }
        if (depth + 1 == indices.size()) elements[index] = value.deepCopy();
        else current = &elements[index];
    }
}

void Interpreter::collectTarget(const Expr& target, std::string& name,
                                std::vector<std::size_t>& indices) {
    if (const auto* variable = dynamic_cast<const VariableExpr*>(&target)) {
        name = variable->name.lexeme;
        return;
    }
    const auto* index = dynamic_cast<const IndexExpr*>(&target);
    if (index == nullptr) throw RuntimeError("invalid assignment target");
    collectTarget(*index->object, name, indices);
    const Value indexValue = evaluate(*index->index);
    if (!indexValue.isNumber() || indexValue.asNumber() < 0 ||
        std::floor(indexValue.asNumber()) != indexValue.asNumber()) {
        throw RuntimeError("array index must be a non-negative integer");
    }
    indices.push_back(static_cast<std::size_t>(indexValue.asNumber()));
}

bool Interpreter::condition(const Expr& expression) {
    const Value value = evaluate(expression);
    if (!value.isBool()) throw RuntimeError("condition must be true or false");
    return value.asBool();
}

std::size_t Interpreter::arrayIndex(const Value& value, std::size_t size) {
    if (!value.isNumber() || value.asNumber() < 0 ||
        std::floor(value.asNumber()) != value.asNumber()) {
        throw RuntimeError("array index must be a non-negative integer");
    }
    const auto index = static_cast<std::size_t>(value.asNumber());
    if (index >= size) {
        throw RuntimeError("index " + std::to_string(index) + " out of range (size " +
                           std::to_string(size) + ")");
    }
    return index;
}

}  // namespace flux
