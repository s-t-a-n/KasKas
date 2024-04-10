#pragma once

#include "Actuator.hpp"
#include "Clock.hpp"
#include "Sensor.hpp"
#include "implementations/Relay.hpp"

class KasKas {
public:
    struct Config {};

    int init() {
        // scratch

        const int pin = 5;
        Relay relay(pin, Relay::OFF);

        return 0;
    }

    int loop() { return 0; }

    KasKas(Config cfg) { m_cfg = cfg; }

    ~KasKas() {}

private:
    Config m_cfg;
};
