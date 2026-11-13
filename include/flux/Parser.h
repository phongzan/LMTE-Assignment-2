#pragma once

#include <cstddef>
#include <initializer_list>
#include <stdexcept>
#include <string>
#include <vector>

#include "flux/AST.h"
#include "flux/Lexer.h"
#include "flux/Token.h"

namespace flux {

// Builds an abstract syntax tree from lexer tokens using recursive descent.
// Syntax errors are accumulated as diagnostics and recovered at line boundaries.
class Parser {
public:
    explicit Parser(std::vector<Token> tokens);

    Program parse();
    const std::vector<Diagnostic>& diagnostics() const;

private:
    // Internal exception used to leave the current production after recording
    // a diagnostic. It is caught by parse() or block() to continue parsing.
    class ParseError : public std::runtime_error {
    public:
        explicit ParseError(const std::string& message) : std::runtime_error(message) {}
    };

    // Statement productions.
    StmtPtr statement();
    StmtPtr outerAssignment();
    StmtPtr expressionStatement();
    StmtPtr showStatement();
    StmtPtr giveStatement();
    StmtPtr rewindStatement();
    StmtPtr whenStatement();
    StmtPtr repeatStatement();
    StmtPtr untilStatement();
    StmtPtr eachStatement();
    StmtPtr functionStatement();
    std::unique_ptr<BlockStmt> block();

    // Expression productions, ordered from lowest to highest precedence.
    ExprPtr expression();
    ExprPtr pipeline();
    ExprPtr logicalOr();
    ExprPtr logicalAnd();
    ExprPtr equality();
    ExprPtr comparison();
    ExprPtr term();
    ExprPtr factor();
    ExprPtr unary();
    ExprPtr postfix();
    ExprPtr primary();
    ExprPtr assignmentTarget();
    std::vector<ExprPtr> argumentList(TokenType closingType);

    // Grammar validation, newline handling and panic-mode recovery.
    bool isAssignmentTarget(const Expr& expression) const;
    void consumeStatementEnd();
    void skipNewlines();
    void synchronize();

    // Primitive token-stream navigation operations.
    bool match(std::initializer_list<TokenType> types);
    bool check(TokenType type) const;
    bool isAtEnd() const;
    const Token& advance();
    const Token& peek() const;
    const Token& previous() const;
    Token consume(TokenType type, const std::string& message);
    ParseError error(const Token& token, const std::string& message);

    // Input, diagnostics and the index of the next token to consume.
    std::vector<Token> tokens_;
    std::vector<Diagnostic> diagnostics_;
    std::size_t current_ = 0;
};

}  // namespace flux
