#pragma once

#include "kaskas/io/peripheral.hpp"
#include "kaskas/io/provider.hpp"
#include "kaskas/io/providers/analogue_value.hpp"
#include "kaskas/io/providers/digital.hpp"

#include <spine/core/debugging.hpp>
#include <spine/core/exception.hpp>
#include <spine/core/time.hpp>
#include <spine/filter/implementations/bandpass.hpp>

namespace kaskas::io {

class DigitalInputPeripheral : public Peripheral {
public:
    struct Config {
        DigitalInput::Config input_cfg;
        time_ms sampling_interval = time_s(1);
    };

public:
    explicit DigitalInputPeripheral(const Config& cfg)
        : Peripheral(cfg.sampling_interval), _cfg(cfg), _input(std::move(_cfg.input_cfg)) {}
    ~DigitalInputPeripheral() override = default;

    void initialize() override {
        _input.initialize();
        update();
        DBG("Analog Sensor initialized. Value: %.2f V", value());
    }

    void update() override { _value = _input.state() ? LogicalState::ON : LogicalState::OFF; }
    void safe_shutdown(bool critical) override {}

    LogicalState value() const { return _value(); }

    DigitalSensor digital_value_provider() const {
        return {[this]() { return this->value(); }};
    }

private:
    const Config _cfg;
    DigitalInput _input;

    LogicalState _value;
};

} // namespace kaskas::io
