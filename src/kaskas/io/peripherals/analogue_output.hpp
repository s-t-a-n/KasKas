#pragma once
#include "kaskas/io/peripheral.hpp"
#include "kaskas/io/provider.hpp"
#include "kaskas/io/providers/analogue.hpp"

#include <float.h>
#include <spine/core/debugging.hpp>
#include <spine/core/exception.hpp>
#include <spine/core/si_units.hpp>

namespace kaskas::io {

class AnalogueOutputPeripheral : public HAL::AnalogueOutput, public Peripheral {
public:
    explicit AnalogueOutputPeripheral(const Config&& cfg, time_ms sampling_speed = time_ms(100))
        : HAL::AnalogueOutput(std::move(cfg)), Peripheral(sampling_speed), _creep_time_on_target({}) {}
    ~AnalogueOutputPeripheral() override = default;

    void initialize() override { HAL::AnalogueOutput::initialize(); }

    void safe_shutdown(bool critical) override { this->fade_to(0.0); }

    void update() override {
        if (!_is_creeping) return;
        const auto creep_delta = _creep_target_value - value();
        if (std::fabs(creep_delta) < std::fabs(_creep_target_increment)) { // on target
            if (value() != _creep_target_value) { // finalize
                _is_creeping = false;
                AnalogueOutput::set_value(_creep_target_value);
            }
            return;
        }
        if (_creep_target_increment < 0 == creep_delta < 0) { // creep without overshoot
            AnalogueOutput::set_value(value() + _creep_target_increment);
        }

        if (_creep_time_on_target.expired()) { // move directly if out of time
            fade_to(_creep_target_value);
            _is_creeping = false;
        }
    }

    AnalogueActuator analogue_output_provider() {
        return {AnalogueActuator::FunctionMap{
            .value_f = [this]() { return this->value(); },
            .set_value_f = [this](double value) { AnalogueOutput::set_value(value); },
            .fade_to_f =
                [this](double setpoint, double increment = 0.1, time_ms increment_interval = time_ms(100)) {
                    HAL::AnalogueOutput::fade_to(setpoint, increment, increment_interval);
                },
            .creep_to_f = [this](double setpoint, time_ms travel_time) { this->creep_to(setpoint, travel_time); }}};
    }

    /// set a target value and let the updater approach the value with a single increment per sample
    void creep_to(const double setpoint, const time_ms travel_time = time_s(1000)) {
        if (value() == setpoint) return;

        _is_creeping = true;
        _creep_target_value = setpoint;
        _creep_time_on_target = spn::core::time::AlarmTimer(travel_time);
        _creep_target_increment = (setpoint - value()) / (travel_time / update_interval()).raw<double>();
    }

private:
    bool _is_creeping = false;
    double _creep_target_value = 0;
    double _creep_target_increment = 0;
    spn::core::time::AlarmTimer _creep_time_on_target;
};

} // namespace kaskas::io