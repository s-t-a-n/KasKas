#pragma once

// #include "Pin.hpp"

#include <spine/core/exception.hpp>
#include <spine/core/time.hpp>
#include <spine/platform/hal.hpp>

using spn::core::Exception;
using spn::core::time::time_ms;
using spn::core::time::Timer;

class Relay {
public:
    struct Config {
        DigitalOutput::Config pin_cfg;
        time_ms backoff_time = 100;

        Config(DigitalOutput::Config&& pin_cfg, time_ms backoff_time = 100)
            : pin_cfg(pin_cfg), backoff_time(backoff_time){};
    };

    void set_state(DigitalState state) {
        // hard protect against flipping relay back on within backoff threshold

        if (state == ON && _backoff_timer && _cfg.backoff_time > 0
            && _backoff_timer->timeSinceLast() < _cfg.backoff_time) {
            spn_throw(Exception("Tried to flip relay within backoff threshold"));
        }

        if (!_backoff_timer && _cfg.backoff_time > 0)
            _backoff_timer = new Timer();

        _pin.set_state(state);
    }

    void initialize(bool active = false) { _pin.initialize(active); }

    Relay(Config&& cfg) : _cfg(cfg), _pin(std::move(_cfg.pin_cfg)) {}
    ~Relay() { delete _backoff_timer; }

private:
    Config _cfg;
    DigitalOutput _pin;
    Timer* _backoff_timer = nullptr;
};
