#pragma once

#include "kaskas/io/provider.hpp"

#include <magic_enum/magic_enum.hpp>
#include <spine/core/assert.hpp>

#include <functional>

namespace kaskas::io {

/// a provider for any sensor that provides a single fp voltage
class AnalogueSensor : public Provider {
public:
    AnalogueSensor(const std::function<double()>& value_f) : _value_f(value_f){};

    double value() const { return _value_f(); }

    std::unique_ptr<prompt::RPCRecipe> rpc_recipe(const std::string_view& recipe_name,
                                                  const std::string_view& root) override {
        using namespace prompt;
        auto model = std::make_unique<RPCRecipe>(
            RPCRecipe(std::string(recipe_name), //
                      {
                          RPCModel(std::string(root),
                                   [this](const OptStringView&) { return RPCResult(std::to_string(value())); }),
                      }));
        return std::move(model);
    }

private:
    const std::function<double()> _value_f;
};

/// a provider for any actuator that needs a single fp voltage
class AnalogueActuator : public Provider {
public:
    struct FunctionMap {
        const std::function<double()> value_f;
        const std::function<void(double)> set_value_f;
        const std::function<void(double, double, time_ms)> fade_to_f;
        const std::function<void(double, time_ms)> creep_to_f;
    };
    AnalogueActuator(const FunctionMap& map) : _map(map){};

    double value() const { return _map.value_f(); }
    void set_value(double value) { _map.set_value_f(value); }
    void fade_to(double setpoint, double increment = 0.1, time_ms increment_interval = time_ms(150)) {
        _map.fade_to_f(setpoint, increment, increment_interval);
    }
    void creep_to(double setpoint, time_ms travel_time) { _map.creep_to_f(setpoint, travel_time); }

    std::unique_ptr<prompt::RPCRecipe> rpc_recipe(const std::string_view& recipe_name, const std::string_view& root) {
        return {};
    }

private:
    FunctionMap _map;
};

} // namespace kaskas::io