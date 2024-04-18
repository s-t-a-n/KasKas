// #pragma once

// #if defined(ARDUINO) && defined(EMBEDDED)
// #    include <Arduino.h>
// #elif defined(ARDUINO)
// #    include <ArduinoFake.h>
// #endif

// struct Pin {
//     uint8_t pin;
//     Pin(uint8_t pin) : pin(pin) {}
// };

// class DigitalInputPin : public Pin {
// public:
//     struct Config {
//         uint8_t pin;
//         bool pull_up = false;

//         Config(uint8_t pin, bool pull_up = false) : pin(pin), pull_up(pull_up){};
//     };

//     DigitalInputPin(Config& cfg) : _pin(pin), _pull_up(pull_up) {}

//     void initialize() {
//         pinMode(pin, INPUT);
//         if (_pull_up)
//             pinMode(pin, INPUT_PULLUP);
//     }

//     int state() { return digitalRead(pin); }

// private:
//     uint8_t _pin;
//     bool _pull_up = false;
// };

// enum DigitalState { OFF, ON };

// class DigitalOutputPin {
// public:
//     struct Config {
//         uint8_t pin;
//         bool active_on_low;

//         Config(uint8_t pin, bool active_on_low = false) : pin(pin), active_on_low(active_on_low){};
//     };

//     DigitalOutputPin(Config& cfg) : Pin(cfg.pin), _active_on_low(cfg.active_on_low) {}

//     void initialize(bool active = false) {
//         pinMode(pin, OUTPUT);
//         set_state(active);
//     }

//     void set_state(DigitalState state) {
//         if (state == true)
//             digitalWrite(pin, !_cfg.active_on_low);
//         else
//             digitalWrite(pin, _cfg._active_on_low);
//         _active = state;
//     }
//     void set_state(bool active) { set_state(static_cast<DigitalState>(active)); }
//     bool state() const { return _active; }

// private:
//     bool _active;
//     const Config _cfg;
// };