#pragma once

#include "flux/Value.h"

namespace flux::operators {

// Implements Flux arithmetic with explicit runtime type validation.
Value add(const Value& left, const Value& right);
Value subtract(const Value& left, const Value& right);
Value multiply(const Value& left, const Value& right);
Value divide(const Value& left, const Value& right);
Value modulo(const Value& left, const Value& right);

// Implements unary numeric and Boolean operations.
Value negate(const Value& value);
Value logicalNot(const Value& value);

// Implements equality and ordered numeric comparisons.
Value equal(const Value& left, const Value& right);
Value less(const Value& left, const Value& right);
Value lessEqual(const Value& left, const Value& right);
Value greater(const Value& left, const Value& right);
Value greaterEqual(const Value& left, const Value& right);

}  // namespace flux::operators
