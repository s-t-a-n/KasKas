#pragma once

#include "kaskas/io/peripheral.hpp"
#include "kaskas/io/provider.hpp"
#include "kaskas/io/providers/analogue.hpp"

#include <DS18B20.h>
#include <spine/core/debugging.hpp>
#include <spine/core/exception.hpp>
#include <spine/core/time.hpp>
#include <spine/filter/filterstack.hpp>
#include <spine/filter/implementations/bandpass.hpp>

namespace kaskas::io {

using BandPass = spn::filter::BandPass<double>;

class DS18B20TempProbe : public FilteredPeripheral {
public:
    struct Config {
        uint8_t pin;
        time_ms sampling_interval = time_s(1);
    };

public:
    DS18B20TempProbe(const Config& cfg) : FilteredPeripheral(cfg.sampling_interval, 1), _cfg(cfg), _ds18b20(_cfg.pin) {
        _fs.attach_filter(BandPass::Broad());
    }
    ~DS18B20TempProbe() override = default;

    void initialize() override {
        _ds18b20_probe_selector = _ds18b20.selectNext();
        if (_ds18b20_probe_selector == 0) {
            spn::throw_exception(spn::assertion_exception("DS18B20TempProbe could not be initialized"));
        }

        update();
        DBGF("DS18B20TempProbe initialized. Temperature: %.2f °C", temperature());
    }

    void update() override { _fs.new_sample(_ds18b20.getTempC()); }

    void safe_shutdown(bool critical) override {}

    double temperature() const { return _fs.value(); }

    AnalogueSensor temperature_provider() const {
        return {[this]() { return this->temperature(); }};
    }

private:
    const Config _cfg;
    DS18B20 _ds18b20;
    uint8_t _ds18b20_probe_selector = 0;
};
} // namespace kaskas::io