#pragma once

#include "kaskas/components/clock.hpp"
#include "kaskas/components/relay.hpp"
#include "kaskas/events.hpp"
#include "kaskas/subsystems/growlights.hpp"

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
    };

public:
    KasKas(Config& cfg)
        : _cfg(cfg), _ctrl(EventSystem(cfg.esc_cfg)), _growlights(Growlights(&_ctrl, cfg.growlights_cfg)) {}

    ~KasKas() {}

    int setup() {
        _growlights.initialize();

        //        _ctrl.schedule(_ctrl.event(Events::BroadSpectrumTurnOn, time_ms(1000), Event::Data()));
        //        _ctrl.schedule(_ctrl.event(Events::BroadSpectrumTurnOff, time_ms(10000), Event::Data()));
        //        _ctrl.schedule(_ctrl.event(Events::BroadSpectrumTurnOn, time_s(20), Event::Data()));
        return 0;
    }
    int loop() {
        _ctrl.loop();
        return 0;
    }

private:
    Config _cfg;
    EventSystem _ctrl;

private:
    Growlights _growlights;
};
