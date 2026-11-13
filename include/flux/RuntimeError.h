#pragma once

#include <stdexcept>
#include <string>

namespace flux {

// Represents an error caused by executing a valid Flux construct with an
// invalid runtime value, such as division by zero or missing history.
class RuntimeError : public std::runtime_error {
public:
    explicit RuntimeError(const std::string& message)
        : std::runtime_error(message) {}
};

}  // namespace flux
