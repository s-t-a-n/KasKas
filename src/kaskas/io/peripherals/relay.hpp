#pragma once

// #include "Pin.hpp"

#include "kaskas/io/peripherals/digital_output.hpp"

// #include <spine/core/exception.hpp>
// #include <spine/core/timers.hpp>
#include "kaskas/io/providers/digital.hpp"

#include <spine/platform/hal.hpp>

namespace kaskas::io {
using spn::core::Exception;
using spn::core::time::Timer;

class Relay : public Peripheral {
public:
    struct Config {
        HAL::DigitalOutput::Config pin_cfg;
        time_ms backoff_time;
        bool throw_on_early_flip = true;
    };

public:
    explicit Relay(const Config&& cfg) : _cfg(cfg), _pin(std::move(_cfg.pin_cfg)) {}

    void initialize() override { _pin.initialize(); }

    void safe_shutdown(bool critical) override { _pin.set_state(LogicalState::OFF); }

    void set_state(LogicalState state) {
        // hard protect against flipping relay back on within backoff threshold
        const auto time_since_last_flip = _backoff_timer->timeSinceLast();
        if (_backoff_timer && _cfg.backoff_time > time_ms(0) && time_since_last_flip < _cfg.backoff_time && state == ON
            && _pin.state() == OFF) {
            if (_cfg.throw_on_early_flip) {
                spn::throw_exception(spn::runtime_exception("Tried to flip relay within backoff threshold"));
            } else {
                DBG("--------------------------------------------------------------------------");
                DBG("Tried to flip relay within backoff threshold: %ims since last flip",
                     time_ms(time_since_last_flip).printable());
                DBG("--------------------------------------------------------------------------");
            }
        }

        if (!_backoff_timer && _cfg.backoff_time > time_s(0))
            _backoff_timer = std::make_optional(Timer());

        _pin.set_state(state);
    }
    void set_state(bool state) { set_state(state ? LogicalState::ON : LogicalState::OFF); }

    LogicalState state() const { return static_cast<LogicalState>(_pin.state()); }

    DigitalActuator state_provider() {
        return {DigitalActuator::FunctionMap{
            .state_f = [this]() { return state(); },
            .set_state_f = [this](LogicalState value) { this->set_state(value); },
        }};
    }

private:
    const Config _cfg;
    DigitalOutput _pin;
    std::optional<Timer> _backoff_timer = std::nullopt;
};
} // namespace kaskas::io
