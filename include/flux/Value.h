#pragma once

#include <memory>
#include <string>
#include <variant>
#include <vector>

namespace flux {

struct Array;

struct MemoryStats {
    std::size_t arraysCreated = 0;
    std::size_t peakArraysLive = 0;
};

// Runtime representation of every Flux value. std::variant provides a
// type-safe tagged union, while arrays are shared and copied before mutation.
class Value {
public:
    using ArrayPtr = std::shared_ptr<Array>;
    using Storage = std::variant<std::monostate, double, std::string, bool, ArrayPtr>;

    Value();
    explicit Value(double value);
    explicit Value(std::string value);
    explicit Value(const char* value);
    explicit Value(bool value);

    // Constructs an array value from an owned list of elements.
    static Value array(std::vector<Value> elements);
    static void resetMemoryStats();
    static MemoryStats memoryStats();

    // Type predicates used before reading a value from the variant.
    bool isNil() const;
    bool isNumber() const;
    bool isString() const;
    bool isBool() const;
    bool isArray() const;
    bool isTruthy() const;

    // Typed accessors throw RuntimeError when the stored type does not match.
    double asNumber() const;
    const std::string& asString() const;
    bool asBool() const;
    const std::vector<Value>& asArray() const;
    std::vector<Value>& asMutableArray();

    // Formatting and copy helpers used by diagnostics, output and assignment.
    std::string typeName() const;
    std::string toString() const;
    Value deepCopy() const;

    friend bool operator==(const Value& left, const Value& right);
    friend bool operator!=(const Value& left, const Value& right) {
        return !(left == right);
    }

private:
    explicit Value(ArrayPtr array);

    Storage storage_;
};

// Heap object referenced by Value::ArrayPtr.
struct Array {
    explicit Array(std::vector<Value> elements);
    ~Array();

    std::vector<Value> elements;
};

}  // namespace flux
