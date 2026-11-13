#include <functional>
#include <iostream>
#include <sstream>
#include <string>
#include <utility>

#include "flux/Interpreter.h"
#include "flux/Lexer.h"
#include "flux/Parser.h"
#include "flux/RuntimeError.h"
#include "flux/SemanticAnalyzer.h"

namespace {

int failures = 0;

void check(bool condition, const std::string& message) {
    if (!condition) {
        ++failures;
        std::cerr << "FAIL: " << message << '\n';
    }
}

std::string run(const std::string& source, const std::string& input = "") {
    flux::Lexer lexer(source);
    auto tokens = lexer.scanTokens();
    if (!lexer.diagnostics().empty()) throw std::runtime_error("test fixture did not lex");

    flux::Parser parser(std::move(tokens));
    auto program = parser.parse();
    if (!parser.diagnostics().empty()) throw std::runtime_error("test fixture did not parse");

    flux::SemanticAnalyzer analyzer;
    analyzer.analyze(program);
    if (!analyzer.diagnostics().empty()) throw std::runtime_error("test fixture was not semantic");

    std::istringstream inputStream(input);
    std::ostringstream output;
    flux::Interpreter interpreter(inputStream, output);
    interpreter.interpret(program);
    return output.str();
}

void expectRuntimeError(const std::string& source, const std::string& fragment,
                        const std::string& message) {
    try {
        run(source);
        check(false, message);
    } catch (const flux::RuntimeError& error) {
        check(std::string(error.what()).find(fragment) != std::string::npos, message);
    }
}

void testBasicPrograms() {
    check(run("show 42\nshow \"Flux\"\nshow true\nshow [1, 2]\n") ==
              "42\nFlux\ntrue\n[1, 2]\n",
          "P1 prints literals and arrays");
    check(run("(2 + 3) * 4 - 6 / 2 -> r\nshow r\n") == "17\n",
          "P2 evaluates arithmetic precedence");
    check(run("show 17 % 5\nshow \"stock: \" + \"low\"\n") ==
              "2\nstock: low\n",
          "P3 evaluates modulo and string joining");
    check(run("0.1 => GST\nshow 200 * GST\n") == "20\n",
          "P4 reads constants");
}

void testControlFlow() {
    check(run("show 5 > 3 and not (2 == 2)\n") == "false\n",
          "P5 evaluates comparison and logic");
    check(run("7 -> stock\nwhen stock == 0 { show \"out\" } orwhen stock < 10 { show \"low\" } otherwise { show \"ok\" }\n") ==
              "low\n",
          "P6 selects the first true branch");
    check(run("0 -> total\nrepeat 4 as i { total + i -> total }\nshow total\n") == "6\n",
          "P7 executes counted loops");
    check(run("1 -> x\nuntil x > 50 { x * 3 -> x }\nshow x\n") == "81\n",
          "P8 executes conditional loops");
    check(run("0 -> sum\neach p in [5, 10, 20] { sum + p -> sum }\nshow sum\n") == "35\n",
          "P9 executes collection loops");
}

void testFunctionsAndPipelines() {
    check(run("fn fact(n) { when n <= 1 { give 1 }\ngive n * fact(n - 1) }\nshow fact(5)\n") == "120\n",
          "P10 supports functions and recursion");
    check(run("fn double(v) { give v * 2 }\nfn add(a, b) { give a + b }\n5 then double then add(1) then show\n") == "11\n",
          "P11 passes pipeline values as the first argument");
    check(run("0 -> count\nfn sell() { outer count + 1 -> count }\nsell()\nsell()\nshow count\n") == "2\n",
          "P12 supports explicit outer writes");
    check(run("0 -> count\nfn f() { 9 -> count\nshow count }\nf()\nshow count\n") == "9\n0\n",
          "P13 keeps normal function writes local");
    check(run(
              "fn makeCounter() {\n"
              "0 -> value\n"
              "fn next() { outer value + 1 -> value\ngive value }\n"
              "give 0\n"
              "}\n"
              "makeCounter()\n"
              "show next()\n"
              "show next()\n") == "1\n2\n",
          "nested functions retain their captured closure environment");
}

void testArraysHistoryAndInput() {
    check(run("[[1, 2], [3, 4]] -> m\n99 -> m[0][1]\nshow m\nshow len(m)\n") ==
              "[[1, 99], [3, 4]]\n2\n",
          "P14 updates nested arrays");
    check(run("[3] -> a\npush(a, 7) -> a\nshow a\nshow len(a)\n") == "[3, 7]\n2\n",
          "P15 implements push and len");
    check(run("120 -> stock\n115 -> stock\n155 -> stock\nshow stock~1\nshow stock~2\nshow history(stock)\n") ==
              "115\n120\n[120, 115, 155]\n",
          "P16 reads complete variable history");
    check(run("120 -> stock\n999 -> stock\nrewind stock\nshow stock\nshow history(stock)\n") ==
              "120\n[120]\n",
          "P17 rewinds the latest value");
    check(run("[1, 2] -> a\na -> b\n9 -> b[0]\nshow a\nshow history(b)\n") ==
              "[1, 2]\n[[1, 2], [9, 2]]\n",
          "P18 preserves array copies and history");
    check(run("ask \"Qty? \" -> q\nshow q * 2\n", "21\n") == "Qty? 42\n",
          "P19 reads and converts numeric input");
}

void testRuntimeErrors() {
    expectRuntimeError("show y\n", "undefined variable 'y'", "E15 rejects undefined names");
    expectRuntimeError("\"a\" * 3 -> s\n", "needs two numbers", "E16 rejects invalid multiply");
    expectRuntimeError("\"Qty: \" + 5 -> s\n", "needs two numbers or two strings",
                       "E17 rejects mixed addition");
    expectRuntimeError("0 -> n\n10 / n -> z\n", "division by zero", "E18 rejects division by zero");
    expectRuntimeError("[1, 2] -> a\nshow a[5]\n", "index 5 out of range",
                       "E19 rejects invalid indices");
    expectRuntimeError("each x in 5 { show x }\n", "expected array",
                       "E20 rejects non-array each values");
    expectRuntimeError("5 then 3\n", "'then' needs a function", "E21 rejects a non-function pipeline");
    expectRuntimeError("1 -> b\nrewind b\n", "no earlier value", "E22 rejects unavailable rewind");
    expectRuntimeError("5 -> b\nshow b~3\n", "requested history is not available",
                       "E23 rejects unavailable history");
    expectRuntimeError("fn f(a) { give a }\nf(1, 2)\n", "expects 1 argument",
                       "E24 checks function arity");
    expectRuntimeError("fn f(n) { give f(n + 1) }\nf(1)\n",
                       "call depth limit (1,000) exceeded",
                       "E25 stops runaway recursion without crashing");
}

void testIntegrationPrograms() {
    check(run(
              "50 -> stock\n"
              "fn sell(qty) { outer stock - qty -> stock }\n"
              "sell(5)\n"
              "sell(60)\n"
              "when stock < 0 { rewind stock }\n"
              "show stock\n"
              "show history(stock)\n") == "45\n[50, 45]\n",
          "P20 detects and rewinds an oversold stock level");

    check(run(
              "[5, 2, 9, 1] -> a\n"
              "repeat len(a) as i {\n"
              "repeat len(a) - 1 as j {\n"
              "when a[j] > a[j + 1] {\n"
              "a[j] -> t\n"
              "a[j + 1] -> a[j]\n"
              "t -> a[j + 1]\n"
              "}\n"
              "}\n"
              "}\n"
              "show a\n") == "[1, 2, 5, 9]\n",
          "P21 integrates nested loops and indexed array writes");

    check(run(
              "fn f() {\n"
              "give 1\n"
              "show \"never\"\n"
              "}\n"
              "show f()\n"
              "show 2 + 3 * 4\n"
              "5 -> x\n"
              "5 -> x\n"
              "show history(x)\n") == "1\n14\n[5, 5]\n",
          "P23 preserves observable history and skips code after give");
}

void testMemoryManagement() {
    flux::Value::resetMemoryStats();
    check(run(
              "repeat 1000 as i {\n"
              "[i, i + 1, i + 2] -> tmp\n"
              "}\n"
              "show \"done\"\n") == "done\n",
          "P22 completes the temporary-array workload");
    const flux::MemoryStats stats = flux::Value::memoryStats();
    check(stats.arraysCreated == 1000,
          "P22 creates exactly 1000 arrays (actual " +
              std::to_string(stats.arraysCreated) + ")");
    check(stats.peakArraysLive == 1,
          "P22 releases each temporary array after its iteration (actual " +
              std::to_string(stats.peakArraysLive) + ")");
}

}  // namespace

int main() {
    testBasicPrograms();
    testControlFlow();
    testFunctionsAndPipelines();
    testArraysHistoryAndInput();
    testRuntimeErrors();
    testIntegrationPrograms();
    testMemoryManagement();

    if (failures == 0) {
        std::cout << "All interpreter tests passed.\n";
        return 0;
    }
    std::cerr << failures << " interpreter test(s) failed.\n";
    return 1;
}
