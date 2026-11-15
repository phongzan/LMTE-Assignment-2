#include "flux/Value.h"

#include <iomanip>
#include <sstream>
#include <utility>

#include "flux/RuntimeError.h"

namespace flux {

namespace {

std::size_t arraysCreated = 0;
std::size_t arraysLive = 0;
std::size_t peakArraysLive = 0;

}  // namespace

Value::Value() = default;
Value::Value(double value) : storage_(value) {}
Value::Value(std::string value) : storage_(std::move(value)) {}
Value::Value(const char* value) : storage_(std::string(value)) {}
Value::Value(bool value) : storage_(value) {}
Value::Value(ArrayPtr array) : storage_(std::move(array)) {}

Value Value::array(std::vector<Value> elements) {
    // Arrays live on the heap so Value objects remain cheap to move and copy.
    return Value(std::make_shared<Array>(std::move(elements)));
}

void Value::resetMemoryStats() {
    arraysCreated = 0;
    arraysLive = 0;
    peakArraysLive = 0;
}

MemoryStats Value::memoryStats() { return {arraysCreated, peakArraysLive}; }

bool Value::isNil() const { return std::holds_alternative<std::monostate>(storage_); }
bool Value::isNumber() const { return std::holds_alternative<double>(storage_); }
bool Value::isString() const { return std::holds_alternative<std::string>(storage_); }
bool Value::isBool() const { return std::holds_alternative<bool>(storage_); }
bool Value::isArray() const { return std::holds_alternative<ArrayPtr>(storage_); }

bool Value::isTruthy() const {
    if (isNil()) return false;
    if (isBool()) return asBool();
    return true;
}

double Value::asNumber() const {
    // Typed accessors fail loudly instead of permitting implicit conversions.
    if (!isNumber()) {
        throw RuntimeError("expected number, got " + typeName());
    }
    return std::get<double>(storage_);
}

const std::string& Value::asString() const {
    if (!isString()) {
        throw RuntimeError("expected string, got " + typeName());
    }
    return std::get<std::string>(storage_);
}

bool Value::asBool() const {
    if (!isBool()) {
        throw RuntimeError("expected bool, got " + typeName());
    }
    return std::get<bool>(storage_);
}

const std::vector<Value>& Value::asArray() const {
    if (!isArray()) {
        throw RuntimeError("expected array, got " + typeName());
    }
    return std::get<ArrayPtr>(storage_)->elements;
}

std::vector<Value>& Value::asMutableArray() {
    // Copy-on-write prevents mutation through one variable from changing another
    // variable that was assigned the same array value.
    if (!isArray()) {
        throw RuntimeError("expected array, got " + typeName());
    }

    auto& array = std::get<ArrayPtr>(storage_);
    if (array.use_count() != 1) {
        std::vector<Value> elements;
        elements.reserve(array->elements.size());
        for (const auto& element : array->elements) {
            elements.push_back(element.deepCopy());
        }
        array = std::make_shared<Array>(std::move(elements));
    }
    return array->elements;
}

std::string Value::typeName() const {
    if (isNil()) return "nil";
    if (isNumber()) return "number";
    if (isString()) return "string";
    if (isBool()) return "bool";
    return "array";
}

std::string Value::toString() const {
    // Produce the user-facing Flux representation, including recursive arrays
    // and numbers without unnecessary trailing zeros.
    if (isNil()) return "nil";
    if (isBool()) return asBool() ? "true" : "false";
    if (isString()) return asString();
    if (isNumber()) {
        std::ostringstream output;
        output << std::setprecision(15) << asNumber();
        return output.str();
    }

    std::ostringstream output;
    output << '[';
    const auto& elements = asArray();
    for (std::size_t index = 0; index < elements.size(); ++index) {
        if (index != 0) output << ", ";
        output << elements[index].toString();
    }
    output << ']';
    return output.str();
}

Value Value::deepCopy() const {
    // Scalars copy directly; arrays recursively duplicate their elements so
    // history entries remain immutable snapshots.
    if (!isArray()) return *this;

    std::vector<Value> copy;
    copy.reserve(asArray().size());
    for (const auto& element : asArray()) {
        copy.push_back(element.deepCopy());
    }
    return Value::array(std::move(copy));
}

bool operator==(const Value& left, const Value& right) {
    // Variant equality handles scalar types; arrays compare element by element.
    if (left.storage_.index() != right.storage_.index()) return false;
    if (left.isNil()) return true;
    if (left.isNumber()) return left.asNumber() == right.asNumber();
    if (left.isString()) return left.asString() == right.asString();
    if (left.isBool()) return left.asBool() == right.asBool();
    return left.asArray() == right.asArray();
}

Array::Array(std::vector<Value> elements) : elements(std::move(elements)) {
    ++arraysCreated;
    ++arraysLive;
    if (arraysLive > peakArraysLive) peakArraysLive = arraysLive;
}

Array::~Array() { --arraysLive; }

}  // namespace flux
