#pragma once

#include "Pin.hpp"

#include <spine/core/exception.hpp>
#include <spine/core/time.hpp>

using spn::core::Exception;
using spn::core::time::time_ms;
using spn::core::time::Timer;

class Relay : public DigitalOutputPin {
public:
    struct Config {
        DigitalOutputPin::Config pin_cfg;
        time_ms backoff_time = 100;

        Config(DigitalOutputPin::Config&& pin_cfg, time_ms backoff_time = 100)
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

        DigitalOutputPin::set_state(state);
    }

    Relay(Config& cfg) : DigitalOutputPin(cfg.pin_cfg), _cfg(cfg) {}
    Relay(Config&& cfg) : DigitalOutputPin(cfg.pin_cfg), _cfg(cfg) {}
    ~Relay() { delete _backoff_timer; }

private:
    Config _cfg;
    Timer* _backoff_timer = nullptr;
};
