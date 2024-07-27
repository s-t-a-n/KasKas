#pragma once

#include "kaskas/io/peripheral.hpp"
#include "kaskas/io/provider.hpp"
#include "kaskas/io/providers/analogue.hpp"

#include <DallasTemperature.h>
#include <OneWire.h>
//
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
    DS18B20TempProbe(const Config& cfg) :
        FilteredPeripheral(cfg.sampling_interval, 1),
        _cfg(cfg),
        _onewire(_cfg.pin),
        _ds18b20(&_onewire) {
        _fs.attach_filter(BandPass::Broad());
    }
    ~DS18B20TempProbe() override = default;

    void initialize() override {
        _ds18b20.begin();

        if (!_ds18b20.getAddress(_ds18b20_address, 0)) {
            spn::throw_exception(spn::assertion_exception("DS18B20TempProbe could not be initialized"));
        }

        // do an update for the first value and change to async
        update();
        _ds18b20.setWaitForConversion(false);

        DBG("DS18B20TempProbe initialized. Temperature: %.2f °C", temperature());
    }

    void update() override {
        if (_ds18b20.isConversionComplete()) {
            _fs.new_sample(_ds18b20.getTempC(_ds18b20_address));
            _ds18b20.requestTemperatures();
        } else
            DBG("DS18B20: update called before conversion is complete");
    }

    void safe_shutdown(bool critical) override {}

    double temperature() const { return _fs.value(); }

    AnalogueSensor temperature_provider() const {
        return {[this]() { return this->temperature(); }};
    }

private:
    const Config _cfg;

    OneWire _onewire;
    DallasTemperature _ds18b20;
    DeviceAddress _ds18b20_address{};
};
} // namespace kaskas::io
