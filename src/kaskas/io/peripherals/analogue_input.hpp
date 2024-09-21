#pragma once
#include "kaskas/io/peripheral.hpp"
#include "kaskas/io/provider.hpp"
#include "kaskas/io/providers/analogue.hpp"

#include <spine/core/debugging.hpp>
#include <spine/core/exception.hpp>
#include <spine/filter/filterstack.hpp>
#include <spine/filter/implementations/bandpass.hpp>
#include <spine/structure/units/si.hpp>

namespace kaskas::io {

class AnalogueInputPeripheral : public FilteredPeripheral {
public:
    using Filter = spn::filter::Filter<double>;

    struct Config {
        AnalogueInput::Config input_cfg;
        time_ms sampling_interval = time_s(1);

        size_t number_of_filters = 0;

        // std::initializer_list<std::unique_ptr<Filter>> filters = {
        //     BandPass::make(BandPass::Config{.mode = BandPass::Mode::RELATIVE,
        //                                     .mantissa = 1,
        //                                     .decades = 0.01,
        //                                     .offset = 0,
        //                                     .rejection_limit = 10,
        //                                     .throw_on_rejection_limit = true})};
    };

public:
    explicit AnalogueInputPeripheral(const Config& cfg)
        : FilteredPeripheral(cfg.sampling_interval, cfg.number_of_filters), _cfg(cfg),
          _input(std::move(_cfg.input_cfg)) {}
    ~AnalogueInputPeripheral() override = default;

    void initialize() override {
        _input.initialize();
        update();
        DBG("Analog Sensor initialized. Raw value: %.2f, Filtered value: %.2f", raw_value(), value());
    }

    void update() override { _fs.new_sample(raw_value()); }
    void safe_shutdown(bool critical) override {}

    double raw_value() const { return _input.read(); }
    double value() const { return _fs.value(); }

    AnalogueSensor analogue_value_provider() const {
        return {[this]() { return this->value(); }};
    }

private:
    const Config _cfg;
    AnalogueInput _input;
};

} // namespace kaskas::io
