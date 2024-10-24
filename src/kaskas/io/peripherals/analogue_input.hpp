#pragma once
#include "kaskas/io/peripheral.hpp"
#include "kaskas/io/providers/analogue.hpp"

#include <spine/core/debugging.hpp>
#include <spine/filter/filterstack.hpp>
#include <spine/structure/units/si.hpp>

namespace kaskas::io {

class AnalogueInputPeripheral : public FilteredPeripheral {
public:
    using Filter = spn::filter::Filter<float>;

    struct Config {
        AnalogueInput::Config input_cfg;
        k_time_ms sampling_interval = k_time_s(1);
        size_t number_of_filters = 0;
        const char* id = nullptr;
    };

public:
    explicit AnalogueInputPeripheral(const Config& cfg)
        : FilteredPeripheral(cfg.sampling_interval, cfg.number_of_filters), _cfg(cfg),
          _input(std::move(_cfg.input_cfg)) {}
    ~AnalogueInputPeripheral() override = default;

    void initialize() override {
        _input.initialize();
        update();
        LOG("Analog sensor {%s} initialized. Raw value: %.2f, Filtered value: %.2f", _cfg.id, raw_value(), value());
    }

    void update() override { _fs.new_sample(raw_value()); }
    void safe_shutdown(bool critical) override {}

    float raw_value() const { return _input.read(); }
    float value() const { return _fs.value(); }

    AnalogueSensor analogue_value_provider() const {
        return {[this]() { return this->value(); }};
    }

private:
    const Config _cfg;
    AnalogueInput _input;
};

} // namespace kaskas::io
