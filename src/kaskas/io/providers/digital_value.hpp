#pragma once

#include "kaskas/io/provider.hpp"

#include <functional>

namespace kaskas::io {

/// a provider for any datasource that provides a single fp value
class DigitalValue : public Provider {
public:
    DigitalValue(const std::function<LogicalState()>& value_f) : _value_f(value_f){};

    LogicalState value() const { return _value_f(); }

private:
    const std::function<LogicalState()> _value_f;
};

} // namespace kaskas::io