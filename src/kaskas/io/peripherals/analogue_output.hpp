#pragma once
#include "kaskas/io/peripheral.hpp"
#include "kaskas/io/providers/analogue.hpp"

#include <spine/core/debugging.hpp>
#include <spine/core/exception.hpp>
#include <spine/structure/units/si.hpp>

namespace kaskas::io {

class AnalogueOutputPeripheral : public HAL::AnalogueOutput, public Peripheral {
public:
    explicit AnalogueOutputPeripheral(const Config&& cfg, k_time_ms sampling_speed = k_time_ms(100))
        : HAL::AnalogueOutput(std::move(cfg)), Peripheral(sampling_speed), _creep_time_on_target({}) {}
    ~AnalogueOutputPeripheral() override = default;

    void initialize() override { HAL::AnalogueOutput::initialize(); }

    void safe_shutdown(bool critical) override { this->fade_to(0.0); }

    void update() override {
        if (!_is_creeping) return;
        const auto creep_delta = _creep_target_value - value();
        if (std::fabs(creep_delta) < std::fabs(_creep_target_increment)) { // on target
            if (value() != _creep_target_value) { // finalize
                creep_stop();
                AnalogueOutput::set_value(_creep_target_value);
            }
            return;
        }
        if (_creep_target_increment < 0 == creep_delta < 0) { // creep without overshoot
            AnalogueOutput::set_value(value() + _creep_target_increment);
        }

        if (_creep_time_on_target.expired()) { // move directly if out of time
            fade_to(_creep_target_value);
            creep_stop();
        }
    }

    AnalogueActuator analogue_output_provider() {
        return {AnalogueActuator::FunctionMap{
            .value_f = [this]() { return this->value(); },
            .set_value_f = [this](float value) { AnalogueOutput::set_value(value); },
            .fade_to_f =
                [this](float setpoint, float increment = 0.1, k_time_ms increment_interval = k_time_ms(100)) {
                    HAL::AnalogueOutput::fade_to(setpoint, increment, increment_interval);
                },
            .creep_to_f = [this](float setpoint, k_time_ms travel_time) { this->creep_to(setpoint, travel_time); },
            .creep_stop_f = [this]() { this->creep_stop(); }}};
    }

    /// Set a creeping target value and let the updater approach the value with a single increment per sample
    void creep_to(const float setpoint, const k_time_ms travel_time = k_time_s(1000)) {
        if (value() == setpoint) return;

        _is_creeping = true;
        _creep_target_value = setpoint;
        _creep_time_on_target = AlarmTimer(travel_time);
        spn_assert((travel_time / update_interval()).raw<float>() != 0);
        _creep_target_increment = (setpoint - value()) / (travel_time / update_interval()).raw<float>();
    }

    /// Stop creeping to target.
    void creep_stop() { _is_creeping = false; }

private:
    using AlarmTimer = spn::structure::time::AlarmTimer;

    bool _is_creeping = false;
    float _creep_target_value = 0;
    float _creep_target_increment = 0;
    AlarmTimer _creep_time_on_target;
};

} // namespace kaskas::io
