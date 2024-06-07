#pragma once

#include "kaskas/io/peripheral.hpp"
#include "kaskas/io/provider.hpp"
#include "kaskas/io/providers/analogue.hpp"

#include <DS18B20.h>
#include <spine/core/debugging.hpp>
#include <spine/core/exception.hpp>
#include <spine/core/time.hpp>
#include <spine/filter/implementations/bandpass.hpp>

namespace kaskas::io {

using BandPass = spn::filter::BandPass<double>;

class DS18B20TempProbe : public Peripheral {
public:
    struct Config {
        uint8_t pin;
        time_ms sampling_interval = time_s(1);

        BandPass::Config filter_cfg = // example medium band pass to reject significant outliers
            BandPass::Config{.mode = BandPass::Mode::RELATIVE, .mantissa = 1, .decades = 0.01, .offset = 0};
    };

public:
    DS18B20TempProbe(const Config& cfg)
        : Peripheral(cfg.sampling_interval), _cfg(cfg), _ds18b20(_cfg.pin), _filter(std::move(_cfg.filter_cfg)) {}
    ~DS18B20TempProbe() override = default;

    void initialize() override {
        _ds18b20_probe_selector = _ds18b20.selectNext();
        if (_ds18b20_probe_selector == 0) {
            dbg::throw_exception(spn::core::assertion_error("DS18B20TempProbe could not be initialized"));
        }

        update();
        DBGF("DS18B20TempProbe initialized. Temperature: %.2f °C", temperature());
    }

    void update() override { _filter.new_reading(_ds18b20.getTempC()); }

    void safe_shutdown(bool critical) override {}

    double temperature() const { return _filter.value(); }

    // DEPRECATED
    // double read() const { return _filter.value(); }

    AnalogueSensor temperature_provider() const {
        return {[this]() { return this->temperature(); }};
    }

private:
    const Config _cfg;
    DS18B20 _ds18b20;
    uint8_t _ds18b20_probe_selector = 0;

    BandPass _filter;
};
} // namespace kaskas::io