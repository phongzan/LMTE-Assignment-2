#pragma once

#include <cstddef>
#include <string>

namespace flux {

// Identifies every lexical category that can be produced by the Flux lexer.
// The groups below represent structural tokens, keywords, operators and delimiters.
enum class TokenType {
    EndOfFile,
    Newline,
    Number,
    String,
    Identifier,

    True,
    False,
    When,
    OrWhen,
    Otherwise,
    Repeat,
    As,
    Until,
    Each,
    In,
    Fn,
    Give,
    Rewind,
    Show,
    Ask,
    Outer,
    Then,
    And,
    Or,
    Not,

    Plus,
    Minus,
    Star,
    Slash,
    Percent,
    Arrow,
    FatArrow,
    EqualEqual,
    BangEqual,
    Less,
    LessEqual,
    Greater,
    GreaterEqual,
    Tilde,

    LeftParen,
    RightParen,
    LeftBracket,
    RightBracket,
    LeftBrace,
    RightBrace,
    Comma,
};

// Stores one token together with its original spelling and source location.
// Line and column information is retained so later stages can report useful errors.
struct Token {
    TokenType type;
    std::string lexeme;
    std::size_t line;
    std::size_t column;
};

// Returns a stable, human-readable name for diagnostics and tests.
const char* tokenTypeName(TokenType type);

}  // namespace flux
