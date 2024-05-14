#pragma once

#include <DS18B20.h>
#include <spine/core/debugging.hpp>
#include <spine/core/exception.hpp>

namespace kaskas::io {

class DS18B20TempProbe {
public:
    struct Config {
        uint8_t pin;
    };

public:
    DS18B20TempProbe(const Config& cfg) : _cfg(cfg), _ds18b20(_cfg.pin) {}

    void initialize() {
        _ds18b20_probe_selector = _ds18b20.selectNext();
        if (_ds18b20_probe_selector == 0) {
            dbg::throw_exception(spn::core::assertion_error("DS18B20TempProbe could not be initialized"));
        }
    }
    double read() { return _ds18b20.getTempC(); }

private:
    const Config _cfg;
    DS18B20 _ds18b20;
    uint8_t _ds18b20_probe_selector = 0;
};
} // namespace kaskas::io