#pragma once

#include "kaskas/components/Pin.hpp"
#include "kaskas/components/Relay.hpp"
#include "kaskas/events.hpp"

#include <spine/core/eventsystem.hpp>
#include <spine/core/exception.hpp>
#include <spine/core/time.hpp>

using spn::core::Event;
using spn::core::EventHandler;
using spn::core::Exception;
using spn::core::time_ms;
using spn::core::time::Timer;

using Events = kaskas::Events;
using EventSystem = spn::core::EventSystem;

class Growlights : public EventHandler {
public:
    struct Config {
        Relay violet_spectrum;
        Relay broad_spectrum;
        time_ms back_off_time; // minimal time between changes
    };

public:
    Growlights(EventSystem* evsys, Config& cfg) : EventHandler(evsys), _cfg(cfg) {
        Serial.println("hello from growlighhts");
    };

    virtual void handle_event(Event& event) {
        Serial.println("GROWLIGHTS RECEIVED EVENT");
        switch (static_cast<Events>(event.id())) {
        case Events::VioletSpectrumTurnOn: _cfg.violet_spectrum.set_state(ON); break;
        case Events::VioletSpectrumTurnOff: _cfg.violet_spectrum.set_state(OFF); break;
        case Events::BroadSpectrumTurnOn: _cfg.broad_spectrum.set_state(ON); break;
        case Events::BroadSpectrumTurnOff: _cfg.broad_spectrum.set_state(OFF); break;
        default: break;
        }
    }

    void initialize() {
        _cfg.broad_spectrum.initialize();
        _cfg.violet_spectrum.initialize();

        evsys()->attach(Events::VioletSpectrumTurnOn, this);
        evsys()->attach(Events::VioletSpectrumTurnOff, this);
        evsys()->attach(Events::BroadSpectrumTurnOn, this);
        evsys()->attach(Events::BroadSpectrumTurnOff, this);
    }

private:
    Config _cfg;
};
