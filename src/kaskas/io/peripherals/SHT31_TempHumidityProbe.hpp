#pragma once

#include "kaskas/io/peripheral.hpp"
#include "kaskas/io/providers/analogue.hpp"

#include <SHT31.h>
#include <spine/core/debugging.hpp>
#include <spine/core/exception.hpp>
#include <spine/filter/filterstack.hpp>
#include <spine/filter/implementations/bandpass.hpp>
#include <spine/structure/time/timers.hpp>

#include <cstdint>

namespace kaskas::io {

using spn::core::time::AlarmTimer;

constexpr bool SHT31_USE_CRC = false; // false, means 'not fast' for Tillaart's SHT31 library; i.e. read with CRC

class SHT31TempHumidityProbe : public Peripheral {
public:
    struct Config {
        uint8_t i2c_address = SHT_DEFAULT_ADDRESS;
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
        Wire.begin();
        Wire.setClock(100000); // per example

        _sht31.begin();

        if (!_sht31.read(SHT31_USE_CRC) || !is_ready()) {
            WARN("SHT31: Probe reports errorcode: %04X with status: %04X", _sht31.getError(), _sht31.readStatus())
            spn::throw_exception(spn::assertion_exception("SHT31TempHumidityProbe could not be initialized"));
        }
        DBG("SHT31TempHumidityProbe initialized. Humidity: %.2f %%, temperature: %.2f °C", read_humidity(),
            read_temperature());

        _sht31.heatOff(); // make sure that probe's internal heating element is turned off
        assert(!_sht31.isHeaterOn());
    }

    void update() override {
        bool has_data = _sht31.dataReady() && _sht31.readData(SHT31_USE_CRC);
        if (!has_data) {
            WARN("SHT31: request was not ready in time.");
            if (!is_ready())
                WARN("SHT31: Probe reports errorcode: %04X with status: %04X", _sht31.getError(), _sht31.readStatus());
        }
        if (!_sht31.requestData()) {
            WARN("SHT31: couldnt request data. Probe reports errorcode: %04X with status: %04X", _sht31.getError(),
                 _sht31.readStatus());
        }

        if (!has_data) return;

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

    bool is_ready() { return _sht31.isConnected() && _sht31.getError() == SHT31_OK; }

private:
    const Config _cfg;
    SHT31 _sht31;

    spn::filter::Stack<double> _temperature_fs;
    spn::filter::Stack<double> _humidity_fs;
};
} // namespace kaskas::io
