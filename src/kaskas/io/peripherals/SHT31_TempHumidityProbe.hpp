#pragma once

#include "kaskas/io/peripheral.hpp"
#include "kaskas/io/providers/analogue.hpp"

#include <SHT31.h>
#include <spine/core/debugging.hpp>
#include <spine/core/exception.hpp>
#include <spine/core/timers.hpp>
#include <spine/filter/filterstack.hpp>
#include <spine/filter/implementations/bandpass.hpp>

#include <cstdint>

namespace kaskas::io {

using spn::core::time::AlarmTimer;

class SHT31TempHumidityProbe : public Peripheral {
public:
    struct Config {
        uint8_t i2c_address = SHT_DEFAULT_ADDRESS;
        time_ms request_timeout = time_ms(100);

        time_ms sampling_interval = time_s(1);
    };

public:
    SHT31TempHumidityProbe(const Config& cfg)
        : Peripheral(cfg.sampling_interval), _cfg(cfg), _sht31(SHT31(_cfg.i2c_address)), _temperature_fs(1),
          _humidity_fs(1) {
        _temperature_fs.attach_filter(BandPass::Broad());
        _humidity_fs.attach_filter(BandPass::Broad());
    }

    void initialize() override {
        TwoWire* wire = &Wire; // not using Spine::HAL since it's I2C implementation must be carefully drafted first
        assert(wire != nullptr);
        Wire.begin();
        Wire.setClock(100000);
        _sht31.begin();

        update();

        if (!is_ready()) {
            spn::throw_exception(spn::assertion_exception("SHT31TempHumidityProbe could not be initialized"));
        }
        DBG("SHT31TempHumidityProbe initialized. Humidity: %.2f %%, temperature: %.2f °C", read_humidity(),
            read_temperature());

        _sht31.heatOff(); // make sure that heater is off
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
                if (success) break;
            }
        }

        assert(!timeout.expired()); // todo: handle this through hardware stack
        if (timeout.expired()) {
            WARN("SHT31TempHumidityProbe: request expired. is the connection okay?");
        }

        _temperature_fs.new_sample(_sht31.getTemperature());
        _humidity_fs.new_sample(_sht31.getHumidity());
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
    double temperature() { return _temperature_fs.value(); }
    double humidity() { return _humidity_fs.value(); }

    AnalogueSensor temperature_provider() {
        return {[this]() { return this->temperature(); }};
    }
    AnalogueSensor humidity_provider() {
        return {[this]() { return this->humidity(); }};
    }

    bool is_ready() {
        if (!_sht31.isConnected()) {
            WARN("SHT31 reports not connected with status: %04X and errorcode: %04X", _sht31.readStatus(),
                 _sht31.getError());
            return false;
        }
        if (const auto error = _sht31.getError(); error != SHT31_OK) {
            WARN("SHT31 reports errorcode: %04X with status: %04X", error, _sht31.readStatus());
            return false;
        }
        return true;
    }

private:
    const Config _cfg;
    SHT31 _sht31;

    spn::filter::Stack<double> _temperature_fs;
    spn::filter::Stack<double> _humidity_fs;
};
} // namespace kaskas::io
