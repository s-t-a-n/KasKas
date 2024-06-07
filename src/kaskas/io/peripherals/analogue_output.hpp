#pragma once
#include "kaskas/io/peripheral.hpp"
#include "kaskas/io/provider.hpp"
#include "kaskas/io/providers/analogue.hpp"

#include <spine/core/debugging.hpp>
#include <spine/core/exception.hpp>
#include <spine/core/time.hpp>

namespace kaskas::io {

class AnalogueOutputPeripheral : public HAL::AnalogueOutput, public Peripheral {
public:
    explicit AnalogueOutputPeripheral(const Config&& cfg) : HAL::AnalogueOutput(std::move(cfg)) {}
    ~AnalogueOutputPeripheral() override = default;

    void initialize() override { HAL::AnalogueOutput::initialize(); }

    void safe_shutdown(bool critical) override { this->fade_to(0.0); }

    AnalogueActuator analogue_output_provider() {
        return {AnalogueActuator::FunctionMap{
            .value_f = [this]() { return this->value(); },
            .set_value_f = [this](double value) { AnalogueOutput::set_value(value); },
            .fade_to_f =
                [this](double setpoint, double increment = 0.1, time_ms increment_interval = time_ms(100)) {
                    HAL::AnalogueOutput::fade_to(setpoint, increment, increment_interval);
                }

        }};
    }
};

} // namespace kaskas::io