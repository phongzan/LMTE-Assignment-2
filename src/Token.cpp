#include "flux/Token.h"

namespace flux {

// Keep token names in one place so diagnostics and tests use identical labels.
const char* tokenTypeName(TokenType type) {
    switch (type) {
        case TokenType::EndOfFile: return "EOF";
        case TokenType::Newline: return "NEWLINE";
        case TokenType::Number: return "NUMBER";
        case TokenType::String: return "STRING";
        case TokenType::Identifier: return "IDENTIFIER";
        case TokenType::True: return "TRUE";
        case TokenType::False: return "FALSE";
        case TokenType::When: return "WHEN";
        case TokenType::OrWhen: return "ORWHEN";
        case TokenType::Otherwise: return "OTHERWISE";
        case TokenType::Repeat: return "REPEAT";
        case TokenType::As: return "AS";
        case TokenType::Until: return "UNTIL";
        case TokenType::Each: return "EACH";
        case TokenType::In: return "IN";
        case TokenType::Fn: return "FN";
        case TokenType::Give: return "GIVE";
        case TokenType::Rewind: return "REWIND";
        case TokenType::Show: return "SHOW";
        case TokenType::Ask: return "ASK";
        case TokenType::Outer: return "OUTER";
        case TokenType::Then: return "THEN";
        case TokenType::And: return "AND";
        case TokenType::Or: return "OR";
        case TokenType::Not: return "NOT";
        case TokenType::Plus: return "PLUS";
        case TokenType::Minus: return "MINUS";
        case TokenType::Star: return "STAR";
        case TokenType::Slash: return "SLASH";
        case TokenType::Percent: return "PERCENT";
        case TokenType::Arrow: return "ARROW";
        case TokenType::FatArrow: return "FAT_ARROW";
        case TokenType::EqualEqual: return "EQUAL_EQUAL";
        case TokenType::BangEqual: return "BANG_EQUAL";
        case TokenType::Less: return "LESS";
        case TokenType::LessEqual: return "LESS_EQUAL";
        case TokenType::Greater: return "GREATER";
        case TokenType::GreaterEqual: return "GREATER_EQUAL";
        case TokenType::Tilde: return "TILDE";
        case TokenType::LeftParen: return "LEFT_PAREN";
        case TokenType::RightParen: return "RIGHT_PAREN";
        case TokenType::LeftBracket: return "LEFT_BRACKET";
        case TokenType::RightBracket: return "RIGHT_BRACKET";
        case TokenType::LeftBrace: return "LEFT_BRACE";
        case TokenType::RightBrace: return "RIGHT_BRACE";
        case TokenType::Comma: return "COMMA";
    }
    return "UNKNOWN";
}

}  // namespace flux
