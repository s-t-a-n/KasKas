#pragma once

#include <SHT31.h>
#include <spine/core/debugging.hpp>
#include <spine/core/exception.hpp>
#include <spine/core/timers.hpp>

#include <AH/STL/cstdint>

namespace kaskas::io {

using spn::core::time::AlarmTimer;

class SHT31TempHumidityProbe {
public:
    struct Config {
        uint8_t i2c_address = SHT_DEFAULT_ADDRESS;
        time_ms request_timeout = time_ms(100);
    };

public:
    SHT31TempHumidityProbe(const Config& cfg) : _cfg(cfg), _sht31(SHT31(_cfg.i2c_address)) {}

    void initialize(TwoWire* wire = &Wire) {
        assert(wire != nullptr);
        Wire.begin();
        Wire.setClock(100000);
        _sht31.begin();

        if (!update() || !is_ready()) {
            dbg::throw_exception(spn::core::assertion_error("SHT31TempHumidityProbe could not be initialized"));
        }
        DBGF("SHT31TempHumidityProbe initialized. Humidity: %f %%, temperature: %f °C", read_humidity(),
             read_temperature());

        // make sure that heater is off
        _sht31.heatOff();
        assert(!_sht31.isHeaterOn());
    }
    double read_temperature() {
        SHT31TempHumidityProbe::update();
        return _sht31.getTemperature();
    }
    double read_humidity() {
        SHT31TempHumidityProbe::update();
        return _sht31.getHumidity();
    }
    bool update() {
        _sht31.readData();
        _sht31.requestData();
        auto timeout = AlarmTimer(HAL::millis() + _cfg.request_timeout);
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
            return false;
        }
        return true;
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
};
} // namespace kaskas::io