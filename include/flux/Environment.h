#pragma once

#include <cstddef>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "flux/Value.h"

namespace flux {

// Stores variable bindings for one lexical scope. Environments form a parent
// chain so functions can read outer values while ordinary writes remain local.
// Each mutable binding retains all assigned values to implement Flux history.
class Environment {
public:
    explicit Environment(std::shared_ptr<Environment> parent = nullptr);

    void defineVariable(const std::string& name, const Value& value);
    void defineConstant(const std::string& name, const Value& value);

    // Normal assignment updates a local binding or creates a new local binding.
    // Setting writeOuter updates an existing binding in a parent environment.
    void assign(const std::string& name, const Value& value, bool writeOuter = false);
    void assignNearest(const std::string& name, const Value& value);

    // Reads current and historical values, or removes the newest history entry.
    const Value& get(const std::string& name) const;
    Value previous(const std::string& name, std::size_t assignmentsAgo) const;
    std::vector<Value> history(const std::string& name) const;
    void rewind(const std::string& name);

    bool hasLocal(const std::string& name) const;
    bool has(const std::string& name) const;

private:
    // A binding is either immutable or a chronological sequence of values.
    struct Binding {
        bool constant = false;
        std::vector<Value> values;
    };

    // Binding lookup helpers used for local, inherited and explicit outer writes.
    Binding* findLocal(const std::string& name);
    const Binding* findLocal(const std::string& name) const;
    Binding* find(const std::string& name);
    const Binding* find(const std::string& name) const;
    Binding* findOuter(const std::string& name);

    static void append(Binding& binding, const std::string& name, const Value& value);

    // The parent scope is shared because function and block scopes can outlive
    // the stack frame that created them.
    std::shared_ptr<Environment> parent_;
    std::unordered_map<std::string, Binding> bindings_;
};

}  // namespace flux
