#pragma once

#include "kaskas/components/clock.hpp"
#include "kaskas/components/relay.hpp"
#include "kaskas/events.hpp"
#include "kaskas/subsystems/fluidsystem.hpp"
#include "kaskas/subsystems/growlights.hpp"
#include "kaskas/subsystems/health.hpp"

#include <spine/core/time.hpp>
#include <spine/eventsystem/eventsystem.hpp>
#include <spine/platform/hal.hpp>

using Event = spn::core::Event;
using EventSystem = spn::core::EventSystem;

class KasKas {
public:
    struct Config {
        EventSystem::Config esc_cfg;
        Growlights::Config growlights_cfg;
        Fluidsystem::Config fluidsystem_cfg;
    };

public:
    explicit KasKas(Config& cfg)
        : _cfg(cfg), //
          _evsys(EventSystem(cfg.esc_cfg)), //
          _growlights(Growlights(&_evsys, cfg.growlights_cfg)), //
          _fluidsystem(&_evsys, cfg.fluidsystem_cfg) {}

    ~KasKas() = default;

    int setup() {
        _growlights.initialize();
        _fluidsystem.initialize();

        return 0;
    }
    int loop() {
        _evsys.loop();
        return 0;
    }

private:
    Config _cfg;
    EventSystem _evsys;

private:
    Growlights _growlights;
    Fluidsystem _fluidsystem;
};
