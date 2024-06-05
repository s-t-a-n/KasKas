#pragma once

#include <spine/core/assert.hpp>

template<typename Probe>
class TemperatureProbe {
public:
    TemperatureProbe(Probe* probe) : _probe(probe) { assert(probe); }

    void initialize() { _probe->initialize(); }
    double read() { return _probe->read_temperature(); }

private:
    Probe* _probe;
};

template<typename Probe>
class HumidityProbe {
public:
    HumidityProbe(Probe* probe) : _probe(probe) { assert(probe); }

    void initialize() { _probe->initialize(); }
    double read() const { return _probe->read_humidity(); }

private:
    Probe* _probe;
};
