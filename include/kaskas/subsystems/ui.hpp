#pragma once

#include "kaskas/components/signaltower.hpp"
#include "kaskas/events.hpp"

#include <spine/eventsystem/eventsystem.hpp>

using spn::eventsystem::EventHandler;

class UI : public EventHandler {
public:
    struct Config {
        Signaltower::Config signaltower_cfg;
    };

public:
    UI(EventSystem* evsys, Config& cfg) : EventHandler(evsys), _cfg(cfg), _signaltower(cfg.signaltower_cfg){};

    void initialize() {
        //
        _signaltower.initialize();
        evsys()->attach(Events::WakeUp, this);
        evsys()->attach(Events::OutOfWater, this);
    }

    void handle_event(Event* event) override {
        //
        switch (static_cast<Events>(event->id())) {
        case Events::WakeUp: {
            //
            DBG("UI: WakeUp");
            _signaltower.signal(Signaltower::State::Green);
            break;
        }
        case Events::OutOfWater: {
            //
            DBG("UI: OutOfWater");
            _signaltower.signal(Signaltower::State::Yellow);
            break;
        }
        default:
            HAL::println(static_cast<int>(event->id()));
            assert(!"Event was not handled!");
            break;
        };
    }

private:
    const Config _cfg;

    Signaltower _signaltower;
};