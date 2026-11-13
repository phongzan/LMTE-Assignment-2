#include "flux/Environment.h"

#include <utility>

#include "flux/RuntimeError.h"

namespace flux {

Environment::Environment(std::shared_ptr<Environment> parent)
    : parent_(std::move(parent)) {}

void Environment::defineVariable(const std::string& name, const Value& value) {
    // A new mutable binding begins its history with a defensive deep copy.
    if (hasLocal(name)) {
        throw RuntimeError("variable '" + name + "' is already defined in this scope");
    }
    // Value arrays use copy-on-write, so sharing the initial snapshot avoids an
    // unnecessary allocation while still protecting stored history.
    bindings_.emplace(name, Binding{false, {value}});
}

void Environment::defineConstant(const std::string& name, const Value& value) {
    // Constants store one value and reject every later write or rewind.
    if (hasLocal(name)) {
        throw RuntimeError("name '" + name + "' is already defined in this scope");
    }
    bindings_.emplace(name, Binding{true, {value}});
}

void Environment::assign(const std::string& name, const Value& value, bool writeOuter) {
    // Explicit outer writes update an existing ancestor. Normal writes update a
    // local name or create a new local binding that shadows its parent.
    if (writeOuter) {
        Binding* binding = findOuter(name);
        if (binding == nullptr) {
            throw RuntimeError("no variable '" + name + "' is assigned outside this scope");
        }
        append(*binding, name, value);
        return;
    }

    if (Binding* local = findLocal(name)) {
        append(*local, name, value);
        return;
    }

    // A write inside a child scope creates a local variable unless `outer` was used.
    defineVariable(name, value);
}

void Environment::assignNearest(const std::string& name, const Value& value) {
    // Control-flow blocks update the nearest existing binding, unlike function
    // bodies where a normal write intentionally creates a local shadow.
    if (Binding* binding = find(name)) {
        append(*binding, name, value);
        return;
    }
    defineVariable(name, value);
}

const Value& Environment::get(const std::string& name) const {
    const Binding* binding = find(name);
    if (binding == nullptr) {
        throw RuntimeError("undefined variable '" + name + "'");
    }
    return binding->values.back();
}

Value Environment::previous(const std::string& name, std::size_t assignmentsAgo) const {
    // Convert "assignments ago" into an index from the end of the history list.
    const Binding* binding = find(name);
    if (binding == nullptr) {
        throw RuntimeError("undefined variable '" + name + "'");
    }
    if (binding->constant) {
        throw RuntimeError("constant '" + name + "' has no history");
    }
    if (assignmentsAgo >= binding->values.size()) {
        throw RuntimeError("'" + name + "' has " + std::to_string(binding->values.size()) +
                           " value(s); requested history is not available");
    }
    return binding->values[binding->values.size() - 1 - assignmentsAgo];
}

std::vector<Value> Environment::history(const std::string& name) const {
    // Return independent copies so callers cannot mutate stored history entries.
    const Binding* binding = find(name);
    if (binding == nullptr) {
        throw RuntimeError("undefined variable '" + name + "'");
    }
    if (binding->constant) {
        throw RuntimeError("constant '" + name + "' has no history");
    }

    std::vector<Value> copy;
    copy.reserve(binding->values.size());
    for (const auto& value : binding->values) {
        copy.push_back(value);
    }
    return copy;
}

void Environment::rewind(const std::string& name) {
    // Undo exactly one assignment while preserving at least the original value.
    Binding* binding = find(name);
    if (binding == nullptr) {
        throw RuntimeError("undefined variable '" + name + "'");
    }
    if (binding->constant) {
        throw RuntimeError("cannot rewind constant '" + name + "'");
    }
    if (binding->values.size() < 2) {
        throw RuntimeError("'" + name + "' has no earlier value");
    }
    binding->values.pop_back();
}

bool Environment::hasLocal(const std::string& name) const {
    return bindings_.find(name) != bindings_.end();
}

bool Environment::has(const std::string& name) const {
    return find(name) != nullptr;
}

Environment::Binding* Environment::findLocal(const std::string& name) {
    const auto iterator = bindings_.find(name);
    return iterator == bindings_.end() ? nullptr : &iterator->second;
}

const Environment::Binding* Environment::findLocal(const std::string& name) const {
    const auto iterator = bindings_.find(name);
    return iterator == bindings_.end() ? nullptr : &iterator->second;
}

Environment::Binding* Environment::find(const std::string& name) {
    if (Binding* local = findLocal(name)) return local;
    return parent_ == nullptr ? nullptr : parent_->find(name);
}

const Environment::Binding* Environment::find(const std::string& name) const {
    if (const Binding* local = findLocal(name)) return local;
    return parent_ == nullptr ? nullptr : parent_->find(name);
}

Environment::Binding* Environment::findOuter(const std::string& name) {
    return parent_ == nullptr ? nullptr : parent_->find(name);
}

void Environment::append(Binding& binding, const std::string& name, const Value& value) {
    // Centralise the constant check and copy policy for every assignment.
    if (binding.constant) {
        throw RuntimeError("cannot assign to constant '" + name + "'");
    }
    binding.values.push_back(value);
}

}  // namespace flux
