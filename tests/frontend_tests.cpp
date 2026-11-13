#include <iostream>
#include <string>
#include <utility>
#include <vector>

#include "flux/AST.h"
#include "flux/Lexer.h"
#include "flux/Parser.h"
#include "flux/Token.h"

namespace {

// The lightweight test harness records every failed expectation and allows all
// front-end cases to run before returning a process exit code.
int failures = 0;

void check(bool condition, const std::string& message) {
    if (!condition) {
        ++failures;
        std::cerr << "FAIL: " << message << '\n';
    }
}

std::vector<flux::Token> lex(const std::string& source) {
    // Common lexer helper used by successful token and parser fixtures.
    flux::Lexer lexer(source);
    auto tokens = lexer.scanTokens();
    check(lexer.diagnostics().empty(), "source should lex without errors");
    return tokens;
}

std::vector<flux::Diagnostic> parseDiagnostics(const std::string& source) {
    // Parse an intentionally invalid fixture and return all recovered errors.
    flux::Lexer lexer(source);
    auto tokens = lexer.scanTokens();
    check(lexer.diagnostics().empty(), "syntax-error fixture should lex without errors");
    flux::Parser parser(std::move(tokens));
    parser.parse();
    return parser.diagnostics();
}

bool containsMessage(const std::vector<flux::Diagnostic>& diagnostics,
                     const std::string& fragment) {
    for (const auto& diagnostic : diagnostics) {
        if (diagnostic.message.find(fragment) != std::string::npos) return true;
    }
    return false;
}

flux::Program parse(const std::string& source) {
    // Parse a successful fixture and assert that neither front-end stage failed.
    auto tokens = lex(source);
    flux::Parser parser(std::move(tokens));
    auto program = parser.parse();
    check(parser.diagnostics().empty(), "source should parse without errors");
    return program;
}

void testLexer() {
    // U1: verify the exact token stream for a reversed assignment.
    const auto tokens = lex("stock - 5 -> stock\n");
    const std::vector<flux::TokenType> expected = {
        flux::TokenType::Identifier, flux::TokenType::Minus,
        flux::TokenType::Number, flux::TokenType::Arrow,
        flux::TokenType::Identifier, flux::TokenType::Newline,
        flux::TokenType::EndOfFile,
    };
    check(tokens.size() == expected.size(), "lexer produces expected token count");
    if (tokens.size() == expected.size()) {
        for (std::size_t index = 0; index < expected.size(); ++index) {
            check(tokens[index].type == expected[index], "lexer token type at index " +
                                                         std::to_string(index));
        }
    }
}

void testTwoCharacterOperators() {
    // U2: ensure a multi-character operator is not split into smaller tokens.
    const auto tokens = lex("x >= 1 -> y\n100 => MAX\n");
    check(tokens[1].type == flux::TokenType::GreaterEqual, "'>=' is one token");
    check(tokens[7].type == flux::TokenType::FatArrow, "'=>' is one token");
}

void testNewlineInsertion() {
    // U3: check logical NEWLINE insertion around blocks and continued expressions.
    const auto tokens = lex("{ show i }\na +\n1\n");
    const std::vector<flux::TokenType> expected = {
        flux::TokenType::LeftBrace, flux::TokenType::Show,
        flux::TokenType::Identifier, flux::TokenType::Newline,
        flux::TokenType::RightBrace, flux::TokenType::Newline,
        flux::TokenType::Identifier, flux::TokenType::Plus,
        flux::TokenType::Number, flux::TokenType::Newline,
        flux::TokenType::EndOfFile,
    };
    check(tokens.size() == expected.size(), "newline insertion produces expected token count");
    if (tokens.size() == expected.size()) {
        for (std::size_t index = 0; index < expected.size(); ++index) {
            check(tokens[index].type == expected[index],
                  "newline insertion token type at index " + std::to_string(index));
        }
    }
}

void testAssignmentAndPrecedence() {
    auto program = parse("(2 + 3) * 4 - 6 / 2 -> result\n");
    check(program.statements.size() == 1, "assignment program has one statement");
    auto* assignment = dynamic_cast<flux::AssignmentStmt*>(program.statements[0].get());
    check(assignment != nullptr, "statement is an assignment");
    if (assignment != nullptr) {
        auto* subtract = dynamic_cast<flux::BinaryExpr*>(assignment->value.get());
        check(subtract != nullptr && subtract->operation.type == flux::TokenType::Minus,
              "subtraction is the outer expression");
    }
}

void testMultiplicationPrecedence() {
    // U4: inspect the AST shape rather than only accepting the source text.
    auto program = parse("2 + 3 * 4\n");
    check(program.statements.size() == 1, "precedence program has one statement");
    if (program.statements.size() != 1) return;

    auto* statement = dynamic_cast<flux::ExpressionStmt*>(program.statements[0].get());
    auto* addition = statement == nullptr
                         ? nullptr
                         : dynamic_cast<flux::BinaryExpr*>(statement->expression.get());
    check(addition != nullptr && addition->operation.type == flux::TokenType::Plus,
          "addition is the outer expression");
    auto* multiplication = addition == nullptr
                               ? nullptr
                               : dynamic_cast<flux::BinaryExpr*>(addition->right.get());
    check(multiplication != nullptr &&
              multiplication->operation.type == flux::TokenType::Star,
          "multiplication binds tighter than addition");
}

void testPipelineArgumentInsertionShape() {
    // U5: verify that a `then` expression retains the intended call step.
    auto program = parse("5 then add(1)\n");
    check(program.statements.size() == 1, "pipeline program has one statement");
    if (program.statements.size() != 1) return;

    auto* statement = dynamic_cast<flux::ExpressionStmt*>(program.statements[0].get());
    auto* pipeline = statement == nullptr
                         ? nullptr
                         : dynamic_cast<flux::PipelineExpr*>(statement->expression.get());
    check(pipeline != nullptr, "then expression produces a pipeline AST node");
    auto* call = pipeline == nullptr ? nullptr : dynamic_cast<flux::CallExpr*>(pipeline->step.get());
    check(call != nullptr, "pipeline step preserves the function call");
    if (call != nullptr) {
        check(call->arguments.size() == 1, "explicit pipeline call argument is retained");
        auto* callee = dynamic_cast<flux::VariableExpr*>(call->callee.get());
        check(callee != nullptr && callee->name.lexeme == "add",
              "pipeline calls the expected function");
    }
}

void testExpressionStatementAlternatives() {
    // U6: the same expression prefix can form a variable, constant or plain expression.
    auto program = parse("x -> y\nx => Y\nx\n");
    check(program.statements.size() == 3,
          "assignment, constant and plain expression produce three statements");
    if (program.statements.size() != 3) return;

    auto* variable = dynamic_cast<flux::AssignmentStmt*>(program.statements[0].get());
    auto* constant = dynamic_cast<flux::AssignmentStmt*>(program.statements[1].get());
    check(variable != nullptr && variable->operation.type == flux::TokenType::Arrow,
          "'->' produces a variable assignment");
    check(constant != nullptr && constant->operation.type == flux::TokenType::FatArrow,
          "'=>' produces a constant declaration");
    check(dynamic_cast<flux::ExpressionStmt*>(program.statements[2].get()) != nullptr,
          "expression without an arrow remains an expression statement");
}

void testLanguageConstructs() {
    // Integration smoke test covering the representative surface syntax together.
    const std::string source =
        "100 => MAX\n"
        "0 -> total\n"
        "repeat 4 as i { total + i -> total }\n"
        "fn double(v) { give v * 2 }\n"
        "when total > 3 { show total } otherwise { show 0 }\n"
        "[1, 2, 3] -> values\n"
        "values then show\n"
        "rewind total\n";

    auto program = parse(source);
    check(program.statements.size() == 8, "parser accepts representative Flux constructs");
    check(dynamic_cast<flux::RepeatStmt*>(program.statements[2].get()) != nullptr,
          "repeat statement parsed");
    check(dynamic_cast<flux::FunctionStmt*>(program.statements[3].get()) != nullptr,
          "function statement parsed");
    check(dynamic_cast<flux::WhenStmt*>(program.statements[4].get()) != nullptr,
          "selection statement parsed");
}

void testDiagnostics() {
    // Basic lexical and syntax failures must be reported without terminating the test.
    flux::Lexer lexer("5 @ 3 -> x\n");
    lexer.scanTokens();
    check(lexer.diagnostics().size() == 1, "invalid character produces one lexical diagnostic");

    auto tokens = lex("repeat 3 { show 1 }\n");
    flux::Parser parser(std::move(tokens));
    parser.parse();
    check(!parser.diagnostics().empty(), "missing 'as' produces a syntax diagnostic");
}

void testPlannedErrorCases() {
    // E2-E8: exercise the error cases promised by the assignment test plan.
    {
        flux::Lexer lexer("show \"hello\n");
        lexer.scanTokens();
        check(containsMessage(lexer.diagnostics(), "unterminated string"),
              "E2 reports an unterminated string");
    }

    check(containsMessage(parseDiagnostics("(4 + ) -> x\n"), "expected an expression"),
          "E3 rejects a missing right operand");
    check(containsMessage(parseDiagnostics("when 1 < 2 { show 1\n"),
                          "expected '}' after block"),
          "E4 rejects a missing closing brace");
    check(containsMessage(parseDiagnostics("repeat 3 { show 1 }\n"),
                          "expected 'as' after repeat count"),
          "E5 rejects a counted loop without 'as'");

    const auto detachedOtherwise =
        parseDiagnostics("when true { show 1 }\notherwise { show 2 }\n");
    check(!detachedOtherwise.empty() && detachedOtherwise.front().line == 2,
          "E6 rejects otherwise on a new line");

    check(containsMessage(parseDiagnostics("ask 5 -> q\n"),
                          "expected a string prompt after 'ask'"),
          "E7 rejects a non-string ask prompt");

    const auto recovery = parseDiagnostics("(4 + ) -> x\nshow \"ok\"\n5 * -> y\n");
    check(recovery.size() == 2,
          "E8 panic-mode recovery reports both independent syntax errors");
    if (recovery.size() == 2) {
        check(recovery[0].line == 1 && recovery[1].line == 3,
              "E8 diagnostics retain the two source line numbers");
    }
}

}  // namespace

int main() {
    // Run every front-end test and fail the executable if any assertion failed.
    testLexer();
    testTwoCharacterOperators();
    testNewlineInsertion();
    testAssignmentAndPrecedence();
    testMultiplicationPrecedence();
    testPipelineArgumentInsertionShape();
    testExpressionStatementAlternatives();
    testLanguageConstructs();
    testDiagnostics();
    testPlannedErrorCases();

    if (failures == 0) {
        std::cout << "All front-end tests passed.\n";
        return 0;
    }
    std::cerr << failures << " front-end test(s) failed.\n";
    return 1;
}
