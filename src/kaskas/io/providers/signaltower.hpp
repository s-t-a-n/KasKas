#pragma once

#include <spine/platform/hal.hpp>

class Signaltower {
public:
    using Pin = unsigned char;

    struct Config {
        DigitalOutput pin_red;
        DigitalOutput pin_yellow;
        DigitalOutput pin_green;
    };

    Signaltower(const Config& cfg) : _cfg(cfg) {}

    void initialize() {
        _cfg.pin_red.initialize();
        _cfg.pin_yellow.initialize();
        _cfg.pin_green.initialize();
    }

    enum class State { Red, Yellow, Green };

    void signal(State state) {
        switch (state) {
        case State::Red:
            _cfg.pin_red.set_state(ON);
            _cfg.pin_yellow.set_state(OFF);
            _cfg.pin_green.set_state(OFF);
            break;

        case State::Yellow:
            _cfg.pin_red.set_state(OFF);
            _cfg.pin_yellow.set_state(ON);
            _cfg.pin_green.set_state(OFF);
            break;
        case State::Green:
            _cfg.pin_red.set_state(OFF);
            _cfg.pin_yellow.set_state(OFF);
            _cfg.pin_green.set_state(ON);
            break;
        default: break;
        }
    }

    void blink() {
        auto new_state = !(_cfg.pin_red.state() || _cfg.pin_yellow.state() || _cfg.pin_green.state());
        _cfg.pin_red.set_state(new_state);
        _cfg.pin_yellow.set_state(new_state);
        _cfg.pin_green.set_state(new_state);
    }

private:
    Config _cfg;
};