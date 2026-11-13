#include "flux/Operators.h"

#include <cmath>
#include <functional>

#include "flux/RuntimeError.h"

namespace flux::operators {

namespace {

// Shared validation keeps arithmetic error messages consistent.
void requireNumbers(const Value& left, const Value& right, const char* operation) {
    if (!left.isNumber() || !right.isNumber()) {
        throw RuntimeError(std::string("'") + operation + "' needs two numbers");
    }
}

Value compareNumbers(const Value& left, const Value& right,
                     const std::function<bool(double, double)>& compare,
                     const char* operation) {
    requireNumbers(left, right, operation);
    return Value(compare(left.asNumber(), right.asNumber()));
}

}  // namespace

Value add(const Value& left, const Value& right) {
    // Addition supports either two numbers or two strings; Flux deliberately
    // refuses implicit conversion between the two types.
    if (left.isNumber() && right.isNumber()) {
        return Value(left.asNumber() + right.asNumber());
    }
    if (left.isString() && right.isString()) {
        return Value(left.asString() + right.asString());
    }
    throw RuntimeError("'+' needs two numbers or two strings");
}

Value subtract(const Value& left, const Value& right) {
    requireNumbers(left, right, "-");
    return Value(left.asNumber() - right.asNumber());
}

Value multiply(const Value& left, const Value& right) {
    requireNumbers(left, right, "*");
    return Value(left.asNumber() * right.asNumber());
}

Value divide(const Value& left, const Value& right) {
    // Division validates both operand types and rejects a zero divisor.
    requireNumbers(left, right, "/");
    if (right.asNumber() == 0.0) {
        throw RuntimeError("division by zero");
    }
    return Value(left.asNumber() / right.asNumber());
}

Value modulo(const Value& left, const Value& right) {
    // Modulo uses floating-point fmod because all Flux numbers are doubles.
    requireNumbers(left, right, "%");
    if (right.asNumber() == 0.0) {
        throw RuntimeError("division by zero");
    }
    return Value(std::fmod(left.asNumber(), right.asNumber()));
}

Value negate(const Value& value) {
    if (!value.isNumber()) {
        throw RuntimeError("unary '-' needs a number");
    }
    return Value(-value.asNumber());
}

Value logicalNot(const Value& value) {
    if (!value.isBool()) {
        throw RuntimeError("'not' needs a bool");
    }
    return Value(!value.asBool());
}

Value equal(const Value& left, const Value& right) {
    return Value(left == right);
}

Value less(const Value& left, const Value& right) {
    return compareNumbers(left, right, std::less<double>{}, "<");
}

Value lessEqual(const Value& left, const Value& right) {
    return compareNumbers(left, right, std::less_equal<double>{}, "<=");
}

Value greater(const Value& left, const Value& right) {
    return compareNumbers(left, right, std::greater<double>{}, ">");
}

Value greaterEqual(const Value& left, const Value& right) {
    return compareNumbers(left, right, std::greater_equal<double>{}, ">=");
}

}  // namespace flux::operators
