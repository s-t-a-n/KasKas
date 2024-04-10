#pragma once

#include <Actuator.hpp>

class Relay : public Actuator {
public:
    enum state_t { ON = 1, OFF = 0 };

    void turn_on() { set_state(ON); }
    void turn_off() { set_state(OFF); }
    void set_state(const bool state) { m_state = static_cast<state_t>(state); }
    void set_state(const state_t state) { m_state = state; }

    bool state() { return m_state; }

    Relay(const int pin, const auto state = OFF) {
        m_pin = pin;
        m_state = state;

        // use initializer list!
    }

private:
    state_t m_state;
    const int m_pin;
};
