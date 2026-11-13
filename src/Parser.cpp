#include "flux/Parser.h"

#include <cstdlib>
#include <limits>
#include <memory>
#include <string>
#include <utility>

namespace flux {

Parser::Parser(std::vector<Token> tokens) : tokens_(std::move(tokens)) {}

Program Parser::parse() {
    // Parse every top-level statement. A failed production records a diagnostic
    // and synchronize() advances to the next safe statement boundary.
    Program program;
    skipNewlines();
    while (!isAtEnd()) {
        try {
            program.statements.push_back(statement());
        } catch (const ParseError&) {
            synchronize();
        }
        skipNewlines();
    }
    return program;
}

const std::vector<Diagnostic>& Parser::diagnostics() const { return diagnostics_; }

StmtPtr Parser::statement() {
    // Keyword-led statements are selected directly; all other statements start
    // as an expression and may become an assignment after an arrow.
    if (match({TokenType::Outer})) return outerAssignment();
    if (match({TokenType::Show})) return showStatement();
    if (match({TokenType::Give})) return giveStatement();
    if (match({TokenType::Rewind})) return rewindStatement();
    if (match({TokenType::When})) return whenStatement();
    if (match({TokenType::Repeat})) return repeatStatement();
    if (match({TokenType::Until})) return untilStatement();
    if (match({TokenType::Each})) return eachStatement();
    if (match({TokenType::Fn})) return functionStatement();
    return expressionStatement();
}

StmtPtr Parser::outerAssignment() {
    ExprPtr value = expression();
    const Token operation = consume(TokenType::Arrow, "expected '->' in outer assignment");
    ExprPtr target = assignmentTarget();
    consumeStatementEnd();
    return std::make_unique<AssignmentStmt>(operation, std::move(value), std::move(target), true);
}

StmtPtr Parser::expressionStatement() {
    // Flux places the assigned value before its target. Parsing the expression
    // first also allows the same prefix to remain a plain expression statement.
    ExprPtr value = expression();
    const Token first = value->token;
    if (match({TokenType::Arrow, TokenType::FatArrow})) {
        const Token operation = previous();
        ExprPtr target = assignmentTarget();
        consumeStatementEnd();
        return std::make_unique<AssignmentStmt>(operation, std::move(value), std::move(target), false);
    }
    consumeStatementEnd();
    return std::make_unique<ExpressionStmt>(first, std::move(value));
}

StmtPtr Parser::showStatement() {
    const Token show = previous();
    ExprPtr value = expression();
    consumeStatementEnd();
    return std::make_unique<ShowStmt>(show, std::move(value));
}

StmtPtr Parser::giveStatement() {
    const Token give = previous();
    ExprPtr value = expression();
    consumeStatementEnd();
    return std::make_unique<GiveStmt>(give, std::move(value));
}

StmtPtr Parser::rewindStatement() {
    const Token rewind = previous();
    const Token name = consume(TokenType::Identifier, "expected variable name after 'rewind'");
    consumeStatementEnd();
    return std::make_unique<RewindStmt>(rewind, name);
}

StmtPtr Parser::whenStatement() {
    // Collect the initial branch, zero or more `orwhen` branches, and an optional
    // `otherwise` block into one selection node.
    const Token when = previous();
    std::vector<WhenBranch> branches;
    ExprPtr condition = expression();
    branches.push_back({std::move(condition), block()});

    while (match({TokenType::OrWhen})) {
        ExprPtr nextCondition = expression();
        branches.push_back({std::move(nextCondition), block()});
    }

    std::unique_ptr<BlockStmt> otherwiseBranch;
    if (match({TokenType::Otherwise})) otherwiseBranch = block();
    consumeStatementEnd();
    return std::make_unique<WhenStmt>(when, std::move(branches), std::move(otherwiseBranch));
}

StmtPtr Parser::repeatStatement() {
    const Token repeat = previous();
    ExprPtr count = expression();
    consume(TokenType::As, "expected 'as' after repeat count");
    const Token variable = consume(TokenType::Identifier, "expected loop variable after 'as'");
    auto body = block();
    consumeStatementEnd();
    return std::make_unique<RepeatStmt>(repeat, std::move(count), variable, std::move(body));
}

StmtPtr Parser::untilStatement() {
    const Token until = previous();
    ExprPtr condition = expression();
    auto body = block();
    consumeStatementEnd();
    return std::make_unique<UntilStmt>(until, std::move(condition), std::move(body));
}

StmtPtr Parser::eachStatement() {
    const Token each = previous();
    const Token variable = consume(TokenType::Identifier, "expected loop variable after 'each'");
    consume(TokenType::In, "expected 'in' after loop variable");
    ExprPtr collection = expression();
    auto body = block();
    consumeStatementEnd();
    return std::make_unique<EachStmt>(each, variable, std::move(collection), std::move(body));
}

StmtPtr Parser::functionStatement() {
    // Parse a named parameter list followed by the function's owned block body.
    const Token fn = previous();
    const Token name = consume(TokenType::Identifier, "expected function name after 'fn'");
    consume(TokenType::LeftParen, "expected '(' after function name");

    std::vector<Token> parameters;
    if (!check(TokenType::RightParen)) {
        do {
            parameters.push_back(consume(TokenType::Identifier, "expected parameter name"));
        } while (match({TokenType::Comma}));
    }
    consume(TokenType::RightParen, "expected ')' after parameters");
    auto body = block();
    consumeStatementEnd();
    return std::make_unique<FunctionStmt>(fn, name, std::move(parameters), std::move(body));
}

std::unique_ptr<BlockStmt> Parser::block() {
    // Parse statements until the matching brace. Local recovery lets one block
    // report multiple independent syntax errors.
    const Token brace = consume(TokenType::LeftBrace, "expected '{' before block");
    std::vector<StmtPtr> statements;
    skipNewlines();
    while (!check(TokenType::RightBrace) && !isAtEnd()) {
        try {
            statements.push_back(statement());
        } catch (const ParseError&) {
            synchronize();
        }
        skipNewlines();
    }
    consume(TokenType::RightBrace, "expected '}' after block");
    return std::make_unique<BlockStmt>(brace, std::move(statements));
}

ExprPtr Parser::expression() { return pipeline(); }

ExprPtr Parser::pipeline() {
    // `then` has the lowest precedence so an entire expression flows into the
    // next function step from left to right.
    ExprPtr expression = logicalOr();
    while (match({TokenType::Then})) {
        const Token thenToken = previous();
        ExprPtr step;
        if (match({TokenType::Show})) {
            step = std::make_unique<VariableExpr>(previous());
        } else {
            step = postfix();
        }
        expression = std::make_unique<PipelineExpr>(
            std::move(expression), thenToken, std::move(step));
    }
    return expression;
}

ExprPtr Parser::logicalOr() {
    // Each precedence function consumes its own operators and delegates tighter
    // operands to the function immediately below it.
    ExprPtr expression = logicalAnd();
    while (match({TokenType::Or})) {
        const Token operation = previous();
        expression = std::make_unique<BinaryExpr>(
            std::move(expression), operation, logicalAnd());
    }
    return expression;
}

ExprPtr Parser::logicalAnd() {
    ExprPtr expression = equality();
    while (match({TokenType::And})) {
        const Token operation = previous();
        expression = std::make_unique<BinaryExpr>(
            std::move(expression), operation, equality());
    }
    return expression;
}

ExprPtr Parser::equality() {
    ExprPtr expression = comparison();
    while (match({TokenType::EqualEqual, TokenType::BangEqual})) {
        const Token operation = previous();
        expression = std::make_unique<BinaryExpr>(
            std::move(expression), operation, comparison());
    }
    return expression;
}

ExprPtr Parser::comparison() {
    ExprPtr expression = term();
    if (match({TokenType::Less, TokenType::LessEqual, TokenType::Greater,
               TokenType::GreaterEqual})) {
        const Token operation = previous();
        expression = std::make_unique<BinaryExpr>(
            std::move(expression), operation, term());
    }
    return expression;
}

ExprPtr Parser::term() {
    ExprPtr expression = factor();
    while (match({TokenType::Plus, TokenType::Minus})) {
        const Token operation = previous();
        expression = std::make_unique<BinaryExpr>(
            std::move(expression), operation, factor());
    }
    return expression;
}

ExprPtr Parser::factor() {
    ExprPtr expression = unary();
    while (match({TokenType::Star, TokenType::Slash, TokenType::Percent})) {
        const Token operation = previous();
        expression = std::make_unique<BinaryExpr>(
            std::move(expression), operation, unary());
    }
    return expression;
}

ExprPtr Parser::unary() {
    if (match({TokenType::Minus, TokenType::Not})) {
        const Token operation = previous();
        return std::make_unique<UnaryExpr>(operation, unary());
    }
    return postfix();
}

ExprPtr Parser::postfix() {
    // Repeated postfix operations build calls, array indexing and history reads
    // on top of a primary expression.
    ExprPtr expression = primary();
    while (true) {
        if (match({TokenType::LeftParen})) {
            std::vector<ExprPtr> arguments = argumentList(TokenType::RightParen);
            const Token closing = consume(TokenType::RightParen, "expected ')' after arguments");
            expression = std::make_unique<CallExpr>(
                std::move(expression), closing, std::move(arguments));
        } else if (match({TokenType::LeftBracket})) {
            const Token bracket = previous();
            ExprPtr index = this->expression();
            consume(TokenType::RightBracket, "expected ']' after index");
            expression = std::make_unique<IndexExpr>(
                std::move(expression), bracket, std::move(index));
        } else if (match({TokenType::Tilde})) {
            const Token tilde = previous();
            const Token count = consume(TokenType::Number, "expected history distance after '~'");
            const double parsed = std::strtod(count.lexeme.c_str(), nullptr);
            if (parsed < 0 || parsed > static_cast<double>(std::numeric_limits<std::size_t>::max()) ||
                parsed != static_cast<double>(static_cast<std::size_t>(parsed))) {
                throw error(count, "history distance must be a non-negative integer");
            }
            expression = std::make_unique<HistoryExpr>(
                std::move(expression), tilde, static_cast<std::size_t>(parsed));
        } else {
            break;
        }
    }
    return expression;
}

ExprPtr Parser::primary() {
    // Primary expressions are atomic values, names, input expressions, arrays
    // and parenthesised expressions.
    if (match({TokenType::False})) return std::make_unique<LiteralExpr>(previous(), Value(false));
    if (match({TokenType::True})) return std::make_unique<LiteralExpr>(previous(), Value(true));
    if (match({TokenType::Number})) {
        return std::make_unique<LiteralExpr>(
            previous(), Value(std::strtod(previous().lexeme.c_str(), nullptr)));
    }
    if (match({TokenType::String})) {
        return std::make_unique<LiteralExpr>(previous(), Value(previous().lexeme));
    }
    if (match({TokenType::Identifier})) return std::make_unique<VariableExpr>(previous());
    if (match({TokenType::Ask})) {
        const Token ask = previous();
        const Token prompt = consume(TokenType::String, "expected a string prompt after 'ask'");
        return std::make_unique<AskExpr>(ask, prompt.lexeme);
    }
    if (match({TokenType::LeftBracket})) {
        const Token bracket = previous();
        std::vector<ExprPtr> elements = argumentList(TokenType::RightBracket);
        consume(TokenType::RightBracket, "expected ']' after array elements");
        return std::make_unique<ArrayExpr>(bracket, std::move(elements));
    }
    if (match({TokenType::LeftParen})) {
        ExprPtr grouped = expression();
        consume(TokenType::RightParen, "expected ')' after expression");
        return grouped;
    }
    throw error(peek(), "expected an expression");
}

ExprPtr Parser::assignmentTarget() {
    // Assignment targets must begin with a name but may continue through nested
    // array indices such as matrix[1][0].
    const Token name = consume(TokenType::Identifier, "expected assignment target");
    ExprPtr target = std::make_unique<VariableExpr>(name);
    while (match({TokenType::LeftBracket})) {
        const Token bracket = previous();
        ExprPtr index = expression();
        consume(TokenType::RightBracket, "expected ']' after target index");
        target = std::make_unique<IndexExpr>(std::move(target), bracket, std::move(index));
    }
    return target;
}

std::vector<ExprPtr> Parser::argumentList(TokenType closingType) {
    std::vector<ExprPtr> arguments;
    if (!check(closingType)) {
        do {
            arguments.push_back(expression());
        } while (match({TokenType::Comma}));
    }
    return arguments;
}

bool Parser::isAssignmentTarget(const Expr& expression) const {
    return dynamic_cast<const VariableExpr*>(&expression) != nullptr ||
           dynamic_cast<const IndexExpr*>(&expression) != nullptr;
}

void Parser::consumeStatementEnd() {
    if (match({TokenType::Newline})) return;
    if (check(TokenType::RightBrace) || isAtEnd()) return;
    throw error(peek(), "expected end of statement");
}

void Parser::skipNewlines() {
    while (match({TokenType::Newline})) {}
}

void Parser::synchronize() {
    // Panic-mode recovery discards the broken statement and resumes after a
    // newline or unmatched closing brace.
    while (!isAtEnd()) {
        if (match({TokenType::Newline})) return;
        // Consume an unmatched closing brace so a top-level syntax error cannot
        // leave the parser on the same token forever.
        if (match({TokenType::RightBrace})) return;
        advance();
    }
}

bool Parser::match(std::initializer_list<TokenType> types) {
    for (const TokenType type : types) {
        if (check(type)) {
            advance();
            return true;
        }
    }
    return false;
}

bool Parser::check(TokenType type) const {
    if (isAtEnd()) return type == TokenType::EndOfFile;
    return peek().type == type;
}

bool Parser::isAtEnd() const { return peek().type == TokenType::EndOfFile; }

const Token& Parser::advance() {
    if (!isAtEnd()) ++current_;
    return previous();
}

const Token& Parser::peek() const { return tokens_[current_]; }

const Token& Parser::previous() const { return tokens_[current_ - 1]; }

Token Parser::consume(TokenType type, const std::string& message) {
    if (check(type)) return advance();
    throw error(peek(), message);
}

Parser::ParseError Parser::error(const Token& token, const std::string& message) {
    diagnostics_.push_back({"Syntax", token.line, token.column, message});
    return ParseError(message);
}

}  // namespace flux
