#pragma once

#include "kaskas/events.hpp"

// Todo: abstract into HAL
#include <DS3231.h>
#include <Wire.h>
//
#include <spine/core/exception.hpp>
#include <spine/core/timers.hpp>
#include <spine/eventsystem/eventsystem.hpp>

using spn::core::Exception;
using spn::core::time::Timer;
using spn::eventsystem::Event;
using spn::eventsystem::EventHandler;

using Events = kaskas::Events;
using EventSystem = spn::core::EventSystem;

class Clock : public EventHandler {
public:
    struct Config {};

public:
    Clock(EventSystem* evsys, Config& cfg) : EventHandler(evsys), _cfg(cfg){};

    virtual void handle_event(Event* event) {
        switch (static_cast<Events>(event->id())) {
        case Events::ClockSync: DBG(""); break;

        default: break;
        }
    }

    void initialize() {
        //
        HAL::I2C::initialize();
        evsys()->attach(Events::ClockSync, this);
    }

private:
    Config _cfg;
};
