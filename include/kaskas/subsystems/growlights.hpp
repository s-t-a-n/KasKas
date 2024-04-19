#pragma once

#include "kaskas/components/clock.hpp"
#include "kaskas/components/relay.hpp"
#include "kaskas/events.hpp"

#include <spine/core/exception.hpp>
#include <spine/core/timers.hpp>
#include <spine/eventsystem/eventsystem.hpp>

#include <AH/STL/cstdint>

using spn::core::Exception;
using spn::core::time::Timer;
using spn::eventsystem::Event;
using spn::eventsystem::EventHandler;

using Events = kaskas::Events;
using EventSystem = spn::core::EventSystem;

class Growlights : public EventHandler {
public:
    struct Config {
        Relay violet_spectrum;
        Relay broad_spectrum;

        int8_t starting_hour;
        int8_t duration_hours;
    };

public:
    Growlights(EventSystem* evsys, Config& cfg) : EventHandler(evsys), _cfg(cfg) { assert(cfg.duration_hours <= 24); };

    virtual void handle_event(Event* event) {
        switch (static_cast<Events>(event->id())) {
        case Events::VioletSpectrumTurnOn:
            if (_cfg.violet_spectrum.state() == OFF) {
                _cfg.violet_spectrum.set_state(ON);
                DBG("Growlights: Turning violet spectrum LEDs: ON");
            }
            break;
        case Events::VioletSpectrumTurnOff:
            if (_cfg.violet_spectrum.state() == ON) {
                _cfg.violet_spectrum.set_state(OFF);
                DBG("Growlights: Turning violet spectrum LEDs: OFF");
            }
            break;
        case Events::BroadSpectrumTurnOn:
            if (_cfg.broad_spectrum.state() == OFF) {
                _cfg.broad_spectrum.set_state(ON);
                DBG("Growlights: Turning broad spectrum LEDs: ON");
            }
            break;
        case Events::BroadSpectrumTurnOff:
            if (_cfg.broad_spectrum.state() == ON) {
                _cfg.broad_spectrum.set_state(OFF);
                DBG("Growlights: Turning broad spectrum LEDs: OFF");
            }
            break;
        case Events::LightCycleStart:
            evsys()->schedule(evsys()->event(Events::BroadSpectrumTurnOn, time_s(10), Event::Data()));
            evsys()->schedule(evsys()->event(Events::VioletSpectrumTurnOn, time_s(15), Event::Data()));
            deploy_lightcycle();
            DBG("Growlights: Starting lightcycle");
            break;
        case Events::LightCycleEnd:
            evsys()->schedule(evsys()->event(Events::BroadSpectrumTurnOff, time_s(10), Event::Data()));
            evsys()->schedule(evsys()->event(Events::VioletSpectrumTurnOff, time_s(15), Event::Data()));
            deploy_lightcycle();
            DBG("Growlights: Reached end of lightcycle");
            break;
        default: break;
        }
    }

    void initialize() {
        _cfg.broad_spectrum.initialize();
        _cfg.violet_spectrum.initialize();

        Clock::initialize();

        evsys()->attach(Events::VioletSpectrumTurnOn, this);
        evsys()->attach(Events::VioletSpectrumTurnOff, this);
        evsys()->attach(Events::BroadSpectrumTurnOn, this);
        evsys()->attach(Events::BroadSpectrumTurnOff, this);
        evsys()->attach(Events::LightCycleStart, this);
        evsys()->attach(Events::LightCycleEnd, this);

        deploy_lightcycle();
    }

private:
    void deploy_lightcycle() {
        // DateTime(uint16_t year, uint8_t month, uint8_t day, uint8_t hour = 0, uint8_t min = 0, uint8_t sec = 0);

        auto today_at = [](const DateTime& n, int8_t hours) {
            return DateTime{n.getYear(), n.getMonth(), n.getDay(), hours, 0, 0};
        };
        auto yesterday_at = [today_at](const DateTime& n, int8_t hours) {
            auto today_epoch = today_at(n, hours).getUnixTime();
            return DateTime{today_epoch - 24 * 60 * 60};
        };
        auto tomorrow_at = [today_at](const DateTime& n, int8_t hours) {
            auto today_epoch = today_at(n, hours).getUnixTime();
            return DateTime{today_epoch + 24 * 60 * 60};
        };

        assert(Clock::is_ready());

        const auto now_dt = Clock::now();
        const auto duration = _cfg.duration_hours * 60 * 60;
        const auto nowtime_s = time_s(now_dt.getUnixTime());

        // corner case: start time rolls around the 00:00 mark
        auto between_days = _cfg.starting_hour + _cfg.duration_hours > 24;
        auto get_starttime_s = [today_at, yesterday_at, tomorrow_at](const DateTime& now_dt, bool between_days,
                                                                     int8_t starting_hour, int8_t duration_hours) {
            if (between_days && now_dt.getHour() < starting_hour + duration_hours - 24) {
                // |0    | 24| |  |
                // start ^ now ^  ^ end
                return time_s{yesterday_at(now_dt, starting_hour).getUnixTime()};
            }
            return time_s{today_at(now_dt, starting_hour).getUnixTime()};
        };

        const auto starttime_s = get_starttime_s(now_dt, between_days, _cfg.starting_hour, _cfg.duration_hours);
        const auto endtime_s = time_s(starttime_s + duration);
        if (nowtime_s > starttime_s && nowtime_s < endtime_s) {
            // now is ON time
            DBG("Growlights: now is ON time, schedule TurnOn's and LightCycleEnd!")
            evsys()->schedule(evsys()->event(Events::BroadSpectrumTurnOn, time_s(10), Event::Data()));
            evsys()->schedule(evsys()->event(Events::VioletSpectrumTurnOn, time_s(15), Event::Data()));
            evsys()->schedule(evsys()->event(Events::LightCycleEnd, endtime_s - nowtime_s, Event::Data()));
            return;
        }
        if (nowtime_s < starttime_s) {
            assert(nowtime_s < endtime_s);
            DBG("Growlights: now is OFF time, scheduling LightCycleStart!")
            evsys()->schedule(evsys()->event(Events::LightCycleStart, starttime_s - nowtime_s, Event::Data()));
            return;
        }
        const auto starttime_tomorrow_s = starttime_s + 24 * 60 * 60;
        if (nowtime_s < starttime_tomorrow_s) {
            DBG("Growlights: now is OFF time, scheduling LightCycleStart for tomorrow!")
            evsys()->schedule(evsys()->event(Events::LightCycleStart, starttime_tomorrow_s - nowtime_s, Event::Data()));
            return;
        }
        assert(!"Growlights: deploy_lightcycle has not send out lightcycle events");
    }

private:
    Config _cfg;
};
