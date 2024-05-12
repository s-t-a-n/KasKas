#pragma once

#include "kaskas/component.hpp"
#include "kaskas/events.hpp"
#include "kaskas/io/implementations/DS3231_RTC_EEPROM.hpp"
#include "kaskas/io/relay.hpp"

#include <kaskas/component.hpp>
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
using kaskas::Component;

class Growlights final : public Component {
public:
    struct Config {
        Relay::Config violet_spectrum_cfg;
        Relay::Config broad_spectrum_cfg;

        time_h starting_hour;
        time_h duration_hours;
    };

public:
    explicit Growlights(Config& cfg) : Growlights(nullptr, cfg) {}
    Growlights(EventSystem* evsys, Config& cfg)
        : Component(evsys), _cfg(cfg), _violet_spectrum(std::move(_cfg.violet_spectrum_cfg)),
          _broad_spectrum(std::move(_cfg.broad_spectrum_cfg)) {
        assert(cfg.duration_hours <= time_h(24));
    };

    void initialize() override {
        _broad_spectrum.initialize();
        _violet_spectrum.initialize();

        Clock::initialize();

        assert(evsys());
        evsys()->attach(Events::VioletSpectrumTurnOn, this);
        evsys()->attach(Events::VioletSpectrumTurnOff, this);
        evsys()->attach(Events::BroadSpectrumTurnOn, this);
        evsys()->attach(Events::BroadSpectrumTurnOff, this);
        evsys()->attach(Events::LightCycleStart, this);
        evsys()->attach(Events::LightCycleEnd, this);

        deploy_lightcycle();
    }

    void safe_shutdown(State state) override {
        _violet_spectrum.set_state(LogicalState::OFF);
        HAL::delay(time_ms(300));
        _broad_spectrum.set_state(LogicalState::OFF);
    }

    void handle_event(const Event& event) override {
        switch (static_cast<Events>(event.id())) {
        case Events::VioletSpectrumTurnOn:
            if (_violet_spectrum.state() == OFF) {
                _violet_spectrum.set_state(ON);
                DBG("Growlights: Turning violet spectrum LEDs: ON");
            }
            break;
        case Events::VioletSpectrumTurnOff:
            if (_violet_spectrum.state() == ON) {
                _violet_spectrum.set_state(OFF);
                DBG("Growlights: Turning violet spectrum LEDs: OFF");
            }
            break;
        case Events::BroadSpectrumTurnOn:
            if (_broad_spectrum.state() == OFF) {
                _broad_spectrum.set_state(ON);
                DBG("Growlights: Turning broad spectrum LEDs: ON");
            }
            break;
        case Events::BroadSpectrumTurnOff:
            if (_broad_spectrum.state() == ON) {
                _broad_spectrum.set_state(OFF);
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
            DBG("Growlights: Reached end of lightcycle");
            deploy_lightcycle();

            break;
        default: assert(!"Event was not handled!"); break;
        }
    }

private:
    void deploy_lightcycle() {
        // DateTime(uint16_t year, uint8_t month, uint8_t day, uint8_t hour = 0, uint8_t min = 0, uint8_t sec = 0);

        auto today_at = [](const DateTime& n, const time_h& hours) {
            return DateTime{n.getYear(), n.getMonth(), n.getDay(), hours.raw<int8_t>(), 0, 0};
        };
        auto yesterday_at = [today_at](const DateTime& n, const time_h& hours) {
            auto today_epoch = today_at(n, hours).getUnixTime();
            return DateTime{today_epoch - 24 * 60 * 60};
        };
        auto tomorrow_at = [today_at](const DateTime& n, const time_h& hours) {
            auto today_epoch = today_at(n, hours).getUnixTime();
            return DateTime{today_epoch + 24 * 60 * 60};
        };

        assert(Clock::is_ready());

        const auto now_dt = Clock::now();
        const auto duration = _cfg.duration_hours;
        const auto nowtime_s = time_s(now_dt.getUnixTime());

        // corner case: start time rolls around the 00:00 mark
        auto between_days = time_h(_cfg.starting_hour) + _cfg.duration_hours > time_h(24);
        auto get_starttime_s = [today_at, yesterday_at, tomorrow_at](const DateTime& now_dt, bool between_days,
                                                                     const time_h& starting_hour,
                                                                     const time_h& duration_hours) {
            if (between_days && time_h(now_dt.getHour()) < starting_hour + duration_hours - time_h(24)) {
                // |0    | 24| |  |
                // start ^ now ^  ^ end
                return time_s{yesterday_at(now_dt, starting_hour).getUnixTime()};
            }
            return time_s{today_at(now_dt, starting_hour).getUnixTime()};
        };

        const auto starttime_s = get_starttime_s(now_dt, between_days, _cfg.starting_hour, _cfg.duration_hours);
        const auto endtime_s = time_s(starttime_s + duration);
        if (nowtime_s >= starttime_s && nowtime_s < endtime_s) {
            // now is ON time
            DBG("Growlights: now is ON time, schedule TurnOn's and LightCycleEnd!");
            evsys()->schedule(evsys()->event(Events::BroadSpectrumTurnOn, time_s(10), Event::Data()));
            evsys()->schedule(evsys()->event(Events::VioletSpectrumTurnOn, time_s(15), Event::Data()));
            evsys()->schedule(evsys()->event(Events::LightCycleEnd, endtime_s - nowtime_s, Event::Data()));
            return;
        }
        if (nowtime_s < starttime_s) {
            assert(nowtime_s < endtime_s);
            {
                char m[256];
                snprintf(m, sizeof(m),
                         "Growlights: now is OFF time, scheduling LightCycleStart at %i:00 in %lli minutes.",
                         DateTime(starttime_s.raw()).getHour(), time_m(starttime_s - nowtime_s).raw());
                DBG(m);
            }
            evsys()->schedule(evsys()->event(Events::LightCycleStart, starttime_s - nowtime_s, Event::Data()));
            return;
        }
        const auto starttime_tomorrow_s = starttime_s + time_d(1);
        if (nowtime_s < starttime_tomorrow_s) {
            {
                char m[256];
                snprintf(m, sizeof(m),
                         "Growlights: now is OFF time, scheduling LightCycleStart for tomorrow at %i:00 in %lli hours.",
                         DateTime(starttime_tomorrow_s.raw()).getHour(), time_h(starttime_s - nowtime_s).raw());
                DBG(m);
            }
            evsys()->schedule(evsys()->event(Events::LightCycleStart, starttime_tomorrow_s - nowtime_s, Event::Data()));
            return;
        }
        assert(!"Growlights: deploy_lightcycle has not send out lightcycle events");
    }

private:
    const Config _cfg;

    Relay _violet_spectrum;
    Relay _broad_spectrum;
};
