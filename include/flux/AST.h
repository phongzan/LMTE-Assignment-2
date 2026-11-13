#pragma once

#include <cstddef>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "flux/Token.h"
#include "flux/Value.h"

namespace flux {

// Base node for all expressions. The representative token supplies source
// location information for later semantic and runtime diagnostics.
struct Expr {
    explicit Expr(Token token) : token(std::move(token)) {}
    virtual ~Expr() = default;
    Token token;
};

using ExprPtr = std::unique_ptr<Expr>;

// Expression nodes represent literals, names, arrays and operators.
struct LiteralExpr final : Expr {
    LiteralExpr(Token token, Value value) : Expr(std::move(token)), value(std::move(value)) {}
    Value value;
};

struct VariableExpr final : Expr {
    explicit VariableExpr(Token name) : Expr(name), name(std::move(name)) {}
    Token name;
};

struct ArrayExpr final : Expr {
    ArrayExpr(Token token, std::vector<ExprPtr> elements)
        : Expr(std::move(token)), elements(std::move(elements)) {}
    std::vector<ExprPtr> elements;
};

struct UnaryExpr final : Expr {
    UnaryExpr(Token operation, ExprPtr right)
        : Expr(operation), operation(std::move(operation)), right(std::move(right)) {}
    Token operation;
    ExprPtr right;
};

struct BinaryExpr final : Expr {
    BinaryExpr(ExprPtr left, Token operation, ExprPtr right)
        : Expr(operation), left(std::move(left)), operation(std::move(operation)),
          right(std::move(right)) {}
    ExprPtr left;
    Token operation;
    ExprPtr right;
};

struct CallExpr final : Expr {
    CallExpr(ExprPtr callee, Token closingParen, std::vector<ExprPtr> arguments)
        : Expr(closingParen), callee(std::move(callee)), arguments(std::move(arguments)) {}
    ExprPtr callee;
    std::vector<ExprPtr> arguments;
};

struct IndexExpr final : Expr {
    IndexExpr(ExprPtr object, Token bracket, ExprPtr index)
        : Expr(bracket), object(std::move(object)), index(std::move(index)) {}
    ExprPtr object;
    ExprPtr index;
};

struct HistoryExpr final : Expr {
    HistoryExpr(ExprPtr object, Token tilde, std::size_t assignmentsAgo)
        : Expr(tilde), object(std::move(object)), assignmentsAgo(assignmentsAgo) {}
    ExprPtr object;
    std::size_t assignmentsAgo;
};

struct AskExpr final : Expr {
    AskExpr(Token ask, std::string prompt)
        : Expr(std::move(ask)), prompt(std::move(prompt)) {}
    std::string prompt;
};

// A pipeline keeps the value on the left and the function step on the right;
// the interpreter later inserts the left value as the first call argument.
struct PipelineExpr final : Expr {
    PipelineExpr(ExprPtr input, Token thenToken, ExprPtr step)
        : Expr(thenToken), input(std::move(input)), step(std::move(step)) {}
    ExprPtr input;
    ExprPtr step;
};

// Base node and owning pointer type for executable statements.
struct Stmt {
    explicit Stmt(Token token) : token(std::move(token)) {}
    virtual ~Stmt() = default;
    Token token;
};

using StmtPtr = std::unique_ptr<Stmt>;

// Simple statements cover expressions, assignments, output, returns and rewind.
struct ExpressionStmt final : Stmt {
    ExpressionStmt(Token token, ExprPtr expression)
        : Stmt(std::move(token)), expression(std::move(expression)) {}
    ExprPtr expression;
};

struct AssignmentStmt final : Stmt {
    AssignmentStmt(Token operation, ExprPtr value, ExprPtr target, bool outer)
        : Stmt(operation), operation(std::move(operation)), value(std::move(value)),
          target(std::move(target)), outer(outer) {}
    Token operation;
    ExprPtr value;
    ExprPtr target;
    bool outer;
};

struct ShowStmt final : Stmt {
    ShowStmt(Token show, ExprPtr expression)
        : Stmt(std::move(show)), expression(std::move(expression)) {}
    ExprPtr expression;
};

struct GiveStmt final : Stmt {
    GiveStmt(Token give, ExprPtr expression)
        : Stmt(std::move(give)), expression(std::move(expression)) {}
    ExprPtr expression;
};

struct RewindStmt final : Stmt {
    RewindStmt(Token rewind, Token name)
        : Stmt(std::move(rewind)), name(std::move(name)) {}
    Token name;
};

// A block owns its ordered statements and introduces a nested execution region.
struct BlockStmt final : Stmt {
    BlockStmt(Token brace, std::vector<StmtPtr> statements)
        : Stmt(std::move(brace)), statements(std::move(statements)) {}
    std::vector<StmtPtr> statements;
};

// Selection stores each condition/body pair plus an optional otherwise branch.
struct WhenBranch {
    ExprPtr condition;
    std::unique_ptr<BlockStmt> body;
};

// Control-flow statement nodes represent Flux selection and the three loop forms.
struct WhenStmt final : Stmt {
    WhenStmt(Token when, std::vector<WhenBranch> branches,
             std::unique_ptr<BlockStmt> otherwiseBranch)
        : Stmt(std::move(when)), branches(std::move(branches)),
          otherwiseBranch(std::move(otherwiseBranch)) {}
    std::vector<WhenBranch> branches;
    std::unique_ptr<BlockStmt> otherwiseBranch;
};

struct RepeatStmt final : Stmt {
    RepeatStmt(Token repeat, ExprPtr count, Token variable, std::unique_ptr<BlockStmt> body)
        : Stmt(std::move(repeat)), count(std::move(count)), variable(std::move(variable)),
          body(std::move(body)) {}
    ExprPtr count;
    Token variable;
    std::unique_ptr<BlockStmt> body;
};

struct UntilStmt final : Stmt {
    UntilStmt(Token until, ExprPtr condition, std::unique_ptr<BlockStmt> body)
        : Stmt(std::move(until)), condition(std::move(condition)), body(std::move(body)) {}
    ExprPtr condition;
    std::unique_ptr<BlockStmt> body;
};

struct EachStmt final : Stmt {
    EachStmt(Token each, Token variable, ExprPtr collection, std::unique_ptr<BlockStmt> body)
        : Stmt(std::move(each)), variable(std::move(variable)),
          collection(std::move(collection)), body(std::move(body)) {}
    Token variable;
    ExprPtr collection;
    std::unique_ptr<BlockStmt> body;
};

// A function declaration owns its parameter names and function body.
struct FunctionStmt final : Stmt {
    FunctionStmt(Token fn, Token name, std::vector<Token> parameters,
                 std::unique_ptr<BlockStmt> body)
        : Stmt(std::move(fn)), name(std::move(name)), parameters(std::move(parameters)),
          body(std::move(body)) {}
    Token name;
    std::vector<Token> parameters;
    std::unique_ptr<BlockStmt> body;
};

// Root AST node returned by the parser for an entire source file.
struct Program {
    std::vector<StmtPtr> statements;
};

}  // namespace flux
