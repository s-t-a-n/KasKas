#pragma once

// #include "Pin.hpp"

#include <spine/core/exception.hpp>
#include <spine/core/timers.hpp>
#include <spine/platform/hal.hpp>

using spn::core::Exception;
using spn::core::time::Timer;

class Relay {
public:
    struct Config {
        DigitalOutput::Config pin_cfg;
        time_ms backoff_time = 100;

        Config(DigitalOutput::Config&& pin_cfg, time_ms backoff_time = 100)
            : pin_cfg(pin_cfg), backoff_time(backoff_time){};
    };

public:
    Relay(Config&& cfg) : _cfg(cfg), _pin(std::move(_cfg.pin_cfg)) {}
    ~Relay() { delete _backoff_timer; }

    void set_state(DigitalState state) {
        // hard protect against flipping relay back on within backoff threshold

        if (_backoff_timer && _cfg.backoff_time > time_ms(0) && _backoff_timer->timeSinceLast() < _cfg.backoff_time
            && state == ON && _pin.state() == OFF) {
            dbg::throw_exception(Exception("Tried to flip relay within backoff threshold"));
        }

        if (!_backoff_timer && _cfg.backoff_time > time_s(0))
            _backoff_timer = new Timer();

        _pin.set_state(state);
    }
    bool state() { return _pin.state(); }

    void initialize(bool active = false) { _pin.initialize(active); }

private:
    Config _cfg;
    DigitalOutput _pin;
    Timer* _backoff_timer = nullptr;
};
