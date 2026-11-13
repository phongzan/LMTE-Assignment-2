#include <functional>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include "flux/Environment.h"
#include "flux/Operators.h"
#include "flux/RuntimeError.h"

namespace {

// Minimal dependency-free harness used while the interpreter is being built.
int failures = 0;

void check(bool condition, const std::string& message) {
    if (!condition) {
        ++failures;
        std::cerr << "FAIL: " << message << '\n';
    }
}

void expectRuntimeError(const std::function<void()>& action, const std::string& message) {
    // A runtime rule passes only when the supplied operation throws RuntimeError.
    try {
        action();
        check(false, message);
    } catch (const flux::RuntimeError&) {
        // Expected.
    }
}

void testHistoryAndRewind() {
    // Verify chronological history reads and undoing the latest assignment.
    flux::Environment environment;
    environment.defineVariable("stock", flux::Value(50.0));
    environment.assign("stock", flux::Value(45.0));
    environment.assign("stock", flux::Value(70.0));

    check(environment.previous("stock", 0) == flux::Value(70.0), "x~0 reads current value");
    check(environment.previous("stock", 1) == flux::Value(45.0), "x~1 reads previous value");
    check(environment.previous("stock", 2) == flux::Value(50.0), "x~2 reads oldest value");
    check(environment.history("stock").size() == 3, "history returns every assignment");

    environment.rewind("stock");
    check(environment.get("stock") == flux::Value(45.0), "rewind restores previous value");
    check(environment.history("stock").size() == 2, "rewind removes latest history entry");
}

void testConstants() {
    // Constants reject reassignment, rewind and history access.
    flux::Environment environment;
    environment.defineConstant("MAX", flux::Value(100.0));

    expectRuntimeError([&] { environment.assign("MAX", flux::Value(200.0)); },
                       "constant reassignment must fail");
    expectRuntimeError([&] { environment.rewind("MAX"); },
                       "constant rewind must fail");
    expectRuntimeError([&] { environment.history("MAX"); },
                       "constants must not expose history");
}

void testScopeAndOuter() {
    // Ordinary writes stay local; an explicit outer write updates the parent scope.
    auto global = std::make_shared<flux::Environment>();
    global->defineVariable("count", flux::Value(0.0));
    flux::Environment functionScope(global);

    functionScope.assign("count", flux::Value(9.0));
    check(functionScope.get("count") == flux::Value(9.0), "normal write creates local binding");
    check(global->get("count") == flux::Value(0.0), "normal write does not change outer binding");

    flux::Environment secondFunction(global);
    secondFunction.assign("count", flux::Value(1.0), true);
    check(global->get("count") == flux::Value(1.0), "outer write changes parent binding");
}

void testArrayCopyOnWrite() {
    // Mutating an assigned array must not change its source variable or old history.
    flux::Environment environment;
    environment.defineVariable("a", flux::Value::array({flux::Value(1.0), flux::Value(2.0)}));
    environment.defineVariable("b", environment.get("a"));

    auto changed = environment.get("b").deepCopy();
    changed.asMutableArray()[0] = flux::Value(9.0);
    environment.assign("b", changed);

    check(environment.get("a").toString() == "[1, 2]", "array assignment preserves source value");
    check(environment.get("b").toString() == "[9, 2]", "array update changes destination value");
    check(environment.history("b").size() == 2, "array element update records new history entry");
}

void testOperators() {
    // Check valid arithmetic/string operations and representative type failures.
    check(flux::operators::add(flux::Value(2.0), flux::Value(3.0)) == flux::Value(5.0),
          "number addition");
    check(flux::operators::add(flux::Value("stock: "), flux::Value("low")) ==
              flux::Value("stock: low"),
          "string joining");
    check(flux::operators::modulo(flux::Value(17.0), flux::Value(5.0)) == flux::Value(2.0),
          "modulo");
    expectRuntimeError(
        [] { flux::operators::divide(flux::Value(10.0), flux::Value(0.0)); },
        "division by zero must fail");
    expectRuntimeError(
        [] { flux::operators::add(flux::Value("qty"), flux::Value(5.0)); },
        "mixed string and number addition must fail");
}

}  // namespace

int main() {
    // Run all runtime component tests and expose the result through the exit code.
    testHistoryAndRewind();
    testConstants();
    testScopeAndOuter();
    testArrayCopyOnWrite();
    testOperators();

    if (failures == 0) {
        std::cout << "All runtime tests passed.\n";
        return 0;
    }

    std::cerr << failures << " runtime test(s) failed.\n";
    return 1;
}
