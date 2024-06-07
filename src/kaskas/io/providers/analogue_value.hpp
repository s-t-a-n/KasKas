#pragma once

#include "kaskas/io/provider.hpp"

#include <spine/core/assert.hpp>

#include <functional>

namespace kaskas::io {

/// a provider for any datasource that provides a single fp value
class AnalogueValue : public Provider {
public:
    AnalogueValue(const std::function<double()>& value_f) : _value_f(value_f){};

    double value() const { return _value_f(); }

private:
    const std::function<double()> _value_f;
};

} // namespace kaskas::io