#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "flux/Token.h"

namespace flux {

// Describes a compiler diagnostic and where it occurred in the source file.
struct Diagnostic {
    std::string stage;
    std::size_t line;
    std::size_t column;
    std::string message;

    std::string toString() const;
};

// Converts raw Flux source text into a linear stream of tokens. The lexer also
// inserts logical NEWLINE tokens and records lexical errors without crashing.
class Lexer {
public:
    explicit Lexer(std::string source);

    std::vector<Token> scanTokens();
    const std::vector<Diagnostic>& diagnostics() const;

private:
    // Character-level navigation helpers.
    bool isAtEnd() const;
    char advance();
    char peek() const;
    char peekNext() const;
    bool match(char expected);

    // Token scanners for general characters and multi-character literals.
    void scanToken();
    void scanNumber();
    void scanIdentifier();
    void scanString();
    void handlePhysicalNewline();
    void addToken(TokenType type);
    void addToken(TokenType type, std::string lexeme);
    void report(std::size_t line, std::size_t column, std::string message);

    // Determines whether a physical line break should become a statement end.
    static bool canEndStatement(TokenType type);

    // Scanner state and accumulated output.
    std::string source_;
    std::vector<Token> tokens_;
    std::vector<Diagnostic> diagnostics_;
    std::size_t start_ = 0;
    std::size_t current_ = 0;
    std::size_t line_ = 1;
    std::size_t column_ = 1;
    std::size_t tokenColumn_ = 1;
};

}  // namespace flux
