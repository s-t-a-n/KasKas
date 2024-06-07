#pragma once

#include "kaskas/io/provider.hpp"

#include <functional>

namespace kaskas::io {

/// a provider for any datasource that provides a single logic level
class DigitalSensor : public Provider {
public:
    DigitalSensor(const std::function<LogicalState()>& value_f) : _value_f(value_f){};

    LogicalState value() const { return _value_f(); }

private:
    const std::function<LogicalState()> _value_f;
};

/// a provider for any actuator that needs a single logic level
class DigitalActuator : public Provider {
public:
    struct FunctionMap {
        const std::function<LogicalState()> state_f;
        const std::function<void(LogicalState)> set_state_f;
    };

    DigitalActuator(const FunctionMap& map) : _map(map){};

    double state() const { return _map.state_f(); }
    void set_state(LogicalState state) { _map.set_state_f(state); }

private:
    FunctionMap _map;
};

} // namespace kaskas::io