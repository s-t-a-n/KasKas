#pragma once
#include "kaskas/io/peripheral.hpp"
#include "kaskas/io/provider.hpp"
#include "kaskas/io/providers/analogue.hpp"

#include <spine/core/debugging.hpp>
#include <spine/core/exception.hpp>
#include <spine/core/time.hpp>
#include <spine/filter/implementations/bandpass.hpp>

namespace kaskas::io {
using BandPass = spn::filter::BandPass<double>;

class AnalogueInputPeripheral : public Peripheral {
public:
    struct Config {
        AnalogueInput::Config input_cfg;
        time_ms sampling_interval = time_s(1);

        BandPass::Config filter_cfg = // example medium band pass to reject significant outliers
            BandPass::Config{.mode = BandPass::Mode::RELATIVE, .mantissa = 1, .decades = 0.01, .offset = 0};
    };

public:
    explicit AnalogueInputPeripheral(const Config& cfg)
        : Peripheral(cfg.sampling_interval), _cfg(cfg), _input(std::move(_cfg.input_cfg)),
          _filter(std::move(_cfg.filter_cfg)) {}
    ~AnalogueInputPeripheral() override = default;

    void initialize() override {
        _input.initialize();
        update();
        DBGF("Analog Sensor initialized. Value: %.2f V", value());
    }

    void update() override { _filter.new_reading(_input.read()); }
    void safe_shutdown(bool critical) override {}

    double value() const { return _filter.value(); }

    AnalogueSensor analogue_value_provider() const {
        return {[this]() { return this->value(); }};
    }

private:
    const Config _cfg;
    AnalogueInput _input;

    BandPass _filter;
};

} // namespace kaskas::io