#include "flux/Lexer.h"

#include <cctype>
#include <sstream>
#include <unordered_map>
#include <utility>

namespace flux {

namespace {

// Reserved words are recognized after scanning an identifier-shaped lexeme.
const std::unordered_map<std::string, TokenType> keywords = {
    {"true", TokenType::True},       {"false", TokenType::False},
    {"when", TokenType::When},       {"orwhen", TokenType::OrWhen},
    {"otherwise", TokenType::Otherwise}, {"repeat", TokenType::Repeat},
    {"as", TokenType::As},           {"until", TokenType::Until},
    {"each", TokenType::Each},       {"in", TokenType::In},
    {"fn", TokenType::Fn},           {"give", TokenType::Give},
    {"rewind", TokenType::Rewind},   {"show", TokenType::Show},
    {"ask", TokenType::Ask},         {"outer", TokenType::Outer},
    {"then", TokenType::Then},       {"and", TokenType::And},
    {"or", TokenType::Or},           {"not", TokenType::Not},
};

bool isIdentifierStart(char character) {
    return std::isalpha(static_cast<unsigned char>(character)) || character == '_';
}

bool isIdentifierPart(char character) {
    return std::isalnum(static_cast<unsigned char>(character)) || character == '_';
}

}  // namespace

std::string Diagnostic::toString() const {
    std::ostringstream output;
    output << stage << " error, line " << line;
    if (column != 0) output << ", column " << column;
    output << ": " << message;
    return output.str();
}

Lexer::Lexer(std::string source) : source_(std::move(source)) {}

std::vector<Token> Lexer::scanTokens() {
    // Scan until the source is exhausted, then synthesize the final statement
    // boundary when appropriate and append the mandatory EOF sentinel.
    while (!isAtEnd()) {
        start_ = current_;
        tokenColumn_ = column_;
        scanToken();
    }

    if (!tokens_.empty() && tokens_.back().type != TokenType::Newline &&
        canEndStatement(tokens_.back().type)) {
        tokens_.push_back({TokenType::Newline, "\\n", line_, column_});
    }
    tokens_.push_back({TokenType::EndOfFile, "", line_, column_});
    return tokens_;
}

const std::vector<Diagnostic>& Lexer::diagnostics() const { return diagnostics_; }

bool Lexer::isAtEnd() const { return current_ >= source_.size(); }

char Lexer::advance() {
    const char character = source_[current_++];
    ++column_;
    return character;
}

char Lexer::peek() const { return isAtEnd() ? '\0' : source_[current_]; }

char Lexer::peekNext() const {
    return current_ + 1 >= source_.size() ? '\0' : source_[current_ + 1];
}

bool Lexer::match(char expected) {
    if (isAtEnd() || source_[current_] != expected) return false;
    ++current_;
    ++column_;
    return true;
}

void Lexer::scanToken() {
    // Dispatch the current character to punctuation handling or a specialised
    // scanner. Two-character operators are matched before their prefixes.
    const char character = advance();
    switch (character) {
        case ' ': case '\t': case '\r': return;
        case '\n': handlePhysicalNewline(); return;
        case '#':
            while (peek() != '\n' && !isAtEnd()) advance();
            return;
        case '+': addToken(TokenType::Plus); return;
        case '-': addToken(match('>') ? TokenType::Arrow : TokenType::Minus); return;
        case '*': addToken(TokenType::Star); return;
        case '/': addToken(TokenType::Slash); return;
        case '%': addToken(TokenType::Percent); return;
        case '=':
            if (match('>')) addToken(TokenType::FatArrow);
            else if (match('=')) addToken(TokenType::EqualEqual);
            else report(line_, tokenColumn_, "unexpected '='; use '==' or '=>'");
            return;
        case '!':
            if (match('=')) addToken(TokenType::BangEqual);
            else report(line_, tokenColumn_, "unexpected '!'; use 'not' or '!='");
            return;
        case '<': addToken(match('=') ? TokenType::LessEqual : TokenType::Less); return;
        case '>': addToken(match('=') ? TokenType::GreaterEqual : TokenType::Greater); return;
        case '~': addToken(TokenType::Tilde); return;
        case '(': addToken(TokenType::LeftParen); return;
        case ')': addToken(TokenType::RightParen); return;
        case '[': addToken(TokenType::LeftBracket); return;
        case ']': addToken(TokenType::RightBracket); return;
        case '{': addToken(TokenType::LeftBrace); return;
        case '}':
            if (!tokens_.empty() && tokens_.back().type != TokenType::Newline &&
                tokens_.back().type != TokenType::LeftBrace) {
                tokens_.push_back({TokenType::Newline, "\\n", line_, tokenColumn_});
            }
            addToken(TokenType::RightBrace);
            return;
        case ',': addToken(TokenType::Comma); return;
        case '"': scanString(); return;
        default:
            if (std::isdigit(static_cast<unsigned char>(character))) scanNumber();
            else if (isIdentifierStart(character)) scanIdentifier();
            else report(line_, tokenColumn_, std::string("unexpected character '") + character + "'");
    }
}

void Lexer::scanNumber() {
    // Flux numbers are decimal values with an optional fractional component.
    while (std::isdigit(static_cast<unsigned char>(peek()))) advance();
    if (peek() == '.' && std::isdigit(static_cast<unsigned char>(peekNext()))) {
        advance();
        while (std::isdigit(static_cast<unsigned char>(peek()))) advance();
    }
    addToken(TokenType::Number);
}

void Lexer::scanIdentifier() {
    // Read a complete word, then classify it as either a keyword or a name.
    while (isIdentifierPart(peek())) advance();
    const std::string text = source_.substr(start_, current_ - start_);
    const auto iterator = keywords.find(text);
    addToken(iterator == keywords.end() ? TokenType::Identifier : iterator->second, text);
}

void Lexer::scanString() {
    // Read a quoted string while decoding supported escapes. Strings cannot
    // cross a physical line, so a newline produces a lexical diagnostic.
    std::string value;
    while (!isAtEnd() && peek() != '"') {
        if (peek() == '\n') {
            report(line_, tokenColumn_, "unterminated string");
            return;
        }
        if (peek() == '\\') {
            advance();
            if (isAtEnd()) break;
            const char escaped = advance();
            if (escaped == 'n') value.push_back('\n');
            else if (escaped == 't') value.push_back('\t');
            else if (escaped == '"') value.push_back('"');
            else if (escaped == '\\') value.push_back('\\');
            else value.push_back(escaped);
        } else {
            value.push_back(advance());
        }
    }

    if (isAtEnd()) {
        report(line_, tokenColumn_, "unterminated string");
        return;
    }
    advance();
    addToken(TokenType::String, std::move(value));
}

void Lexer::handlePhysicalNewline() {
    // Physical lines become grammar-visible NEWLINE tokens only when the
    // preceding token can legally finish a statement.
    if (!tokens_.empty() && tokens_.back().type != TokenType::Newline &&
        canEndStatement(tokens_.back().type)) {
        tokens_.push_back({TokenType::Newline, "\\n", line_, tokenColumn_});
    }
    ++line_;
    column_ = 1;
}

void Lexer::addToken(TokenType type) {
    addToken(type, source_.substr(start_, current_ - start_));
}

void Lexer::addToken(TokenType type, std::string lexeme) {
    tokens_.push_back({type, std::move(lexeme), line_, tokenColumn_});
}

void Lexer::report(std::size_t line, std::size_t column, std::string message) {
    diagnostics_.push_back({"Lexical", line, column, std::move(message)});
}

bool Lexer::canEndStatement(TokenType type) {
    // Tokens that form complete values, close grouped values, or terminate a
    // `then show` pipeline are valid statement endings.
    switch (type) {
        case TokenType::Number:
        case TokenType::String:
        case TokenType::Identifier:
        case TokenType::True:
        case TokenType::False:
        // `show` can terminate a pipeline (`values then show`) even though it
        // is a keyword rather than an identifier or literal.
        case TokenType::Show:
        case TokenType::RightParen:
        case TokenType::RightBracket:
        case TokenType::RightBrace:
            return true;
        default:
            return false;
    }
}

}  // namespace flux
