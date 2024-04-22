#pragma once

#include <AH/STL/utility>

constexpr auto DefaultSensorValue = -1.234567890;

template<typename SensorSource, typename FilterType>
class Sensor {
public:
    struct Config {
        using Sensor = SensorSource;
        using Filter = FilterType;

        Sensor sensor;
        Filter filter;
    };

    Sensor(Config&& cfg) : _cfg(std::move(cfg)), _src(std::move(cfg.sensor)), _flt(std::move(cfg.filter)) {
        //
    }

    void update() { _value = _flt.value(_src.read()); }

    double value() {
        assert(_value != DefaultSensorValue);
        return _value;
    }

private:
    Config _cfg;

    SensorSource _src;
    FilterType _flt;

    double _value = DefaultSensorValue;
};