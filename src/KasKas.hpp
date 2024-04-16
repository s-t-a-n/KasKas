#pragma once

#include "components/Clock.hpp"
#include "components/Relay.hpp"
// #include "spine/core/eventsystem.hpp"

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
