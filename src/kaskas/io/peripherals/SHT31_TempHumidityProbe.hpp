#pragma once

#include "kaskas/io/peripheral.hpp"
#include "kaskas/io/providers/analogue.hpp"

#include <SHT31.h>
#include <spine/core/debugging.hpp>
#include <spine/core/exception.hpp>
#include <spine/core/timers.hpp>
#include <spine/filter/implementations/bandpass.hpp>

#include <AH/STL/cstdint>

namespace kaskas::io {

using spn::core::time::AlarmTimer;

class SHT31TempHumidityProbe : public Peripheral {
public:
    struct Config {
        uint8_t i2c_address = SHT_DEFAULT_ADDRESS;
        time_ms request_timeout = time_ms(100);

        time_ms sampling_interval = time_s(1);

        BandPass::Config filter_cfg = // example medium band pass to reject significant outliers
            BandPass::Config{.mode = BandPass::Mode::RELATIVE, .mantissa = 1, .decades = 0.01, .offset = 0};
    };

public:
    SHT31TempHumidityProbe(const Config& cfg)
        : Peripheral(cfg.sampling_interval), _cfg(cfg), _sht31(SHT31(_cfg.i2c_address)),
          _temperature_filter(std::move(_cfg.filter_cfg)), _humidity_filter(std::move(_cfg.filter_cfg)) {}

    void initialize() override {
        // DBGF("initializing SHT31TempHumidityProbe");
        TwoWire* wire = &Wire;
        assert(wire != nullptr);
        Wire.begin();
        Wire.setClock(100000);
        _sht31.begin();

        update();

        if (!is_ready()) {
            dbg::throw_exception(spn::core::assertion_error("SHT31TempHumidityProbe could not be initialized"));
        }
        DBGF("SHT31TempHumidityProbe initialized. Humidity: %f %%, temperature: %f °C", read_humidity(),
             read_temperature());

        // make sure that heater is off
        _sht31.heatOff();
        assert(!_sht31.isHeaterOn());
    }

    void update() override {
        _sht31.readData();
        _sht31.requestData();
        auto timeout = AlarmTimer(_cfg.request_timeout);
        while (!timeout.expired()) {
            delay(1);
            if (_sht31.dataReady()) {
                bool success = _sht31.readData();
                _sht31.requestData();
                if (success)
                    break;
            }
        }

        if (timeout.expired()) {
            DBGF("SHT31TempHumidityProbe: request expired. is the connection okay?");

            // todo: handle this through hardware stack
            assert(!"SHT31TempHumidityProbe: request expired. is the connection okay?");
        }
    }
    void safe_shutdown(bool critical) override {
        _sht31.heatOff(); // make sure that heater is off
    }

    double read_temperature() {
        SHT31TempHumidityProbe::update();
        return temperature();
    }
    double read_humidity() {
        SHT31TempHumidityProbe::update();
        return humidity();
    }
    double temperature() { return _sht31.getTemperature(); }
    double humidity() { return _sht31.getHumidity(); }

    AnalogueSensor temperature_provider() {
        return {[this]() { return this->temperature(); }};
    }
    AnalogueSensor humidity_provider() {
        return {[this]() { return this->humidity(); }};
    }

    bool is_ready() {
        if (!_sht31.isConnected()) {
            DBGF("SHT31 reports not connected with status: %04X and errorcode: %04X", _sht31.readStatus(),
                 _sht31.getError());
            return false;
        }
        if (const auto error = _sht31.getError(); error != SHT31_OK) {
            DBGF("SHT31 reports errorcode: %04X with status: %04X", error, _sht31.readStatus());
            return false;
        }
        return true;
    }

private:
    const Config _cfg;
    SHT31 _sht31;

    BandPass _temperature_filter;
    BandPass _humidity_filter;
};
} // namespace kaskas::io