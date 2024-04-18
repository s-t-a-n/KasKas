#pragma once

#include "kaskas/components/Clock.hpp"
#include "kaskas/components/Relay.hpp"
#include "kaskas/events.hpp"
#include "kaskas/subsystems/growlights.hpp"

#include <spine/core/eventsystem.hpp>

// using Events = KasKas::Events;
using Event = spn::core::Event;
using EventSystemController = spn::core::EventSystemController;

class KasKas {
public:
    struct Config {
        EventSystemController::Config esc_cfg;
        Growlights::Config growlights_cfg;
    };

public:
    KasKas(Config cfg)
        : _cfg(cfg), _ctrl(EventSystemController(cfg.esc_cfg)),
          _growlights(Growlights(_ctrl.eventSystem(), cfg.growlights_cfg)) {}

    ~KasKas() {}

    int setup() {
        Serial.println("1");
        _growlights.initialize();
        Serial.println("2");
        delay(1000);
        auto event = _ctrl.new_event(Events::BroadSpectrumTurnOn, time_ms(200), Event::Data());
        _ctrl.schedule(event);

        event = _ctrl.new_event(Events::BroadSpectrumTurnOff, time_ms(250), Event::Data());
        _ctrl.schedule(event);

        event = _ctrl.new_event(Events::BroadSpectrumTurnOn, time_ms(300), Event::Data());
        _ctrl.schedule(event);

        Serial.println(int(event));
        Serial.println("3");
        delay(1000);

        Serial.println("4");

        // debug_print(__FILE__, __LINE__, __func__, _ctrl.eventSystem()->to_string().c_str());
        return 0;
    }
    int loop() {
        _ctrl.loop();
        return 0;
    }

private:
    Config _cfg;
    EventSystemController _ctrl;

private:
    Growlights _growlights;
};
