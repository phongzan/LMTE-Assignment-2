#include <iostream>
#include <string>
#include <utility>
#include <vector>

#include "flux/Lexer.h"
#include "flux/Parser.h"
#include "flux/SemanticAnalyzer.h"

namespace {

int failures = 0;

void check(bool condition, const std::string& message) {
    if (!condition) {
        ++failures;
        std::cerr << "FAIL: " << message << '\n';
    }
}

std::vector<flux::Diagnostic> analyze(const std::string& source) {
    flux::Lexer lexer(source);
    auto tokens = lexer.scanTokens();
    check(lexer.diagnostics().empty(), "semantic fixture should lex without errors");

    flux::Parser parser(std::move(tokens));
    auto program = parser.parse();
    check(parser.diagnostics().empty(), "semantic fixture should parse without errors");

    flux::SemanticAnalyzer analyzer;
    analyzer.analyze(program);
    return analyzer.diagnostics();
}

bool contains(const std::vector<flux::Diagnostic>& diagnostics,
              const std::string& fragment) {
    for (const auto& diagnostic : diagnostics) {
        if (diagnostic.message.find(fragment) != std::string::npos) return true;
    }
    return false;
}

void testConstantRules() {
    check(contains(analyze("1 => K\n2 -> K\n"), "cannot assign to constant K"),
          "E9 rejects assignment to a constant");
    check(contains(analyze("0.1 => GST\nrewind GST\n"),
                   "cannot rewind constant GST"),
          "E10 rejects rewinding a constant");
}

void testFunctionRules() {
    check(contains(analyze("give 5\n"), "'give' outside a function"),
          "E11 rejects give outside a function");
    check(contains(analyze("fn f(a, a) { give a }\n"),
                   "duplicate parameter 'a' in f"),
          "E13 rejects duplicate parameter names");
}

void testConditionAndOuterRules() {
    check(contains(analyze("until 3 { show 1 }\n"),
                   "condition must be true or false"),
          "E12 rejects an obviously non-Boolean condition");
    check(contains(analyze("fn f() { outer y + 1 -> y }\n"),
                   "no variable 'y' is assigned outside f"),
          "E14 rejects an outer write without an outer binding");
}

void testValidProgram() {
    const auto diagnostics = analyze(
        "0 -> count\n"
        "fn sell(qty) { outer count - qty -> count }\n"
        "when count == 0 { show count }\n");
    check(diagnostics.empty(), "valid constants, conditions and outer writes are accepted");
}

}  // namespace

int main() {
    testConstantRules();
    testFunctionRules();
    testConditionAndOuterRules();
    testValidProgram();

    if (failures == 0) {
        std::cout << "All semantic tests passed.\n";
        return 0;
    }
    std::cerr << failures << " semantic test(s) failed.\n";
    return 1;
}
