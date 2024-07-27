#pragma once

#include "kaskas/component.hpp"
#include "kaskas/events.hpp"
#include "kaskas/io/hardware_stack.hpp"
#include "kaskas/io/providers/digital.hpp"

#include <kaskas/component.hpp>
#include <spine/core/exception.hpp>
#include <spine/core/schedule.hpp>
#include <spine/core/timers.hpp>
#include <spine/core/utils/time_repr.hpp>
#include <spine/eventsystem/eventsystem.hpp>

#include <cstdint>

namespace kaskas::component {
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
        io::HardwareStack::Idx redblue_spectrum_actuator_idx;
        Schedule::Config redblue_spectrum_schedule;

        io::HardwareStack::Idx full_spectrum_actuator_idx;
        Schedule::Config full_spectrum_schedule;

        io::HardwareStack::Idx clock_idx;
    };

public:
    Growlights(io::HardwareStack& hws, const Config& cfg) : Growlights(hws, nullptr, cfg) {}
    Growlights(io::HardwareStack& hws, EventSystem* evsys, const Config& cfg) :
        Component(evsys, hws),
        _cfg(cfg),
        _redblue_spectrum(_hws.digital_actuator(_cfg.redblue_spectrum_actuator_idx)),
        _redblue_spectrum_schedule(std::move(_cfg.redblue_spectrum_schedule)),
        _full_spectrum(_hws.digital_actuator(_cfg.full_spectrum_actuator_idx)),
        _full_spectrum_schedule(std::move(_cfg.full_spectrum_schedule)),
        _clock(_hws.clock(_cfg.clock_idx)){};

    void initialize() override {
        assert(evsys());
        evsys()->attach(Events::LightFullSpectrumCycleCheck, this);
        evsys()->attach(Events::LightFullSpectrumTurnOn, this);
        evsys()->attach(Events::LightFullSpectrumTurnOff, this);
        evsys()->attach(Events::LightRedBlueSpectrumCycleCheck, this);
        evsys()->attach(Events::LightRedBlueSpectrumTurnOn, this);
        evsys()->attach(Events::LightRedBlueSpectrumTurnOff, this);

        auto time_from_now = time_s(5);
        DBG("Growlights: Scheduling LightFullSpectrumCycleCheck in %u seconds.", time_from_now.printable());
        evsys()->schedule(evsys()->event(Events::LightFullSpectrumCycleCheck, time_from_now));
        time_from_now += time_s(5);
        DBG("Growlights: Scheduling LightRedBlueSpectrumCycleCheck in %u seconds.", time_from_now.printable());
        evsys()->schedule(evsys()->event(Events::LightRedBlueSpectrumCycleCheck, time_from_now));
    }

    void safe_shutdown(State state) override {
        HAL::delay(time_ms(300));
        _redblue_spectrum.set_state(LogicalState::OFF);
        HAL::delay(time_ms(300));
        _full_spectrum.set_state(LogicalState::OFF);
    }

    void handle_event(const Event& event) override {
        const auto now_s = [&]() {
            assert(_clock.is_ready());
            const auto now_dt = _clock.now();
            return time_s(time_h(now_dt.getHour())) + time_m(now_dt.getMinute());
        };
        switch (static_cast<Events>(event.id())) {
        case Events::LightFullSpectrumCycleCheck: {
            const auto now = now_s();
            const auto next_setpoint = _full_spectrum_schedule.value_at(now);

            if (next_setpoint > 0 && _full_spectrum.value() == 0) {
                DBG("Growlights: Check: full spectrum is currently off, turn on");
                evsys()->trigger(evsys()->event(Events::LightFullSpectrumTurnOn, time_s(1)));
            } else if (next_setpoint == 0 && _full_spectrum.value() > 0) {
                DBG("Growlights: Check: full spectrum is currently on, turn off");
                evsys()->trigger(evsys()->event(Events::LightFullSpectrumTurnOff, time_s(1)));
            }

            const auto time_until_next_check = _full_spectrum_schedule.start_of_next_block(now) - now;
            assert(_full_spectrum_schedule.start_of_next_block(now) > now);

            DBG("Growlights: Checked. Scheduling next check in %u m", time_m(time_until_next_check).printable());
            assert(time_until_next_check.raw<>() > 0);
            evsys()->schedule(evsys()->event(Events::LightFullSpectrumCycleCheck, time_until_next_check));
            break;
        }
        case Events::LightFullSpectrumTurnOn: {
            DBG("Growlights: LightFullSpectrumTurnOn.");
            _full_spectrum.set_state(LogicalState::ON);
            break;
        }
        case Events::LightFullSpectrumTurnOff: {
            DBG("Growlights: LightFullSpectrumTurnOff.");
            _full_spectrum.set_state(LogicalState::OFF);
            break;
        }

        case Events::LightRedBlueSpectrumCycleCheck: {
            const auto now = now_s();
            const auto next_setpoint = _redblue_spectrum_schedule.value_at(now);

            if (next_setpoint > 0 && _redblue_spectrum.value() == 0) {
                DBG("Growlights: Check: red/blue spectrum is currently off, turn on");
                evsys()->trigger(evsys()->event(Events::LightRedBlueSpectrumTurnOn, time_s(1)));
            } else if (next_setpoint == 0 && _redblue_spectrum.value() > 0) {
                DBG("Growlights: Check: red/blue spectrum is currently on, turn off");
                evsys()->trigger(evsys()->event(Events::LightRedBlueSpectrumTurnOff, time_s(1)));
            }

            const auto time_until_next_check = _redblue_spectrum_schedule.start_of_next_block(now) - now;
            assert(_redblue_spectrum_schedule.start_of_next_block(now) > now);

            DBG("Growlights: Checked. Scheduling next check in %u m", time_m(time_until_next_check).printable());
            assert(time_until_next_check.raw<>() > 0);
            evsys()->schedule(evsys()->event(Events::LightRedBlueSpectrumCycleCheck, time_until_next_check));
            break;
        }
        case Events::LightRedBlueSpectrumTurnOn: {
            DBG("Growlights: LightRedBlueSpectrumTurnOn.");
            _redblue_spectrum.set_state(LogicalState::ON);
            break;
        }
        case Events::LightRedBlueSpectrumTurnOff: {
            DBG("Growlights: LightRedBlueSpectrumTurnOff.");
            _redblue_spectrum.set_state(LogicalState::OFF);
            break;
        }

        default: assert(!"Event was not handled!"); break;
        }
    }

    std::unique_ptr<prompt::RPCRecipe> rpc_recipe() override {
        using namespace prompt;
        auto model = std::make_unique<RPCRecipe>(
            RPCRecipe("Growlights", //
                      {
                          RPCModel(
                              "turnOn",

                              [this](const OptStringView& unit_of_time) {
                                  if (!unit_of_time.has_value()) {
                                      // Handle missing value case
                                      evsys()->trigger(Events::LightFullSpectrumTurnOn);
                                      evsys()->schedule(Events::LightRedBlueSpectrumTurnOn, time_s(5));
                                      return RPCResult(RPCResult::Status::OK);
                                  }

                                  auto time_variant = spn::core::parse_time(unit_of_time.value());
                                  if (!time_variant.has_value()) {
                                      return RPCResult("Error: Invalid time format.");
                                  }

                                  std::visit(
                                      [this](auto&& time) {
                                          evsys()->trigger(Events::LightFullSpectrumTurnOn);
                                          evsys()->schedule(Events::LightRedBlueSpectrumTurnOn, time_s(5));
                                          evsys()->schedule(Events::LightFullSpectrumTurnOff, time);
                                          evsys()->schedule(Events::LightRedBlueSpectrumTurnOff, time + time_s(5));
                                      },
                                      time_variant.value());
                                  return RPCResult(RPCResult::Status::OK);
                              },
                              "Args: length and unit of time, eg. 10s or 1d"),
                          RPCModel("turnOff",

                                   [this](const OptStringView& unit_of_time) {
                                       if (!unit_of_time.has_value()) {
                                           // Handle missing value case
                                           evsys()->trigger(Events::LightRedBlueSpectrumTurnOff);
                                           evsys()->schedule(Events::LightFullSpectrumTurnOff, time_s(5));
                                           return RPCResult(RPCResult::Status::OK);
                                       }

                                       return RPCResult("cannot time turnOff", RPCResult::Status::BAD_INPUT);
                                   }),
                      }));
        return std::move(model);
    }
    void sideload_providers(io::VirtualStackFactory& ssf) override {}

private:
private:
    // void deploy_lightcycle() {
    //     // DateTime(uint16_t year, uint8_t month, uint8_t day, uint8_t hour = 0, uint8_t min = 0, uint8_t sec = 0);
    //
    //     auto today_at = [](const DateTime& n, const time_h& hours) {
    //         return DateTime{n.getYear(), n.getMonth(), n.getDay(), hours.raw<int8_t>(), 0, 0};
    //     };
    //     auto yesterday_at = [today_at](const DateTime& n, const time_h& hours) {
    //         auto today_epoch = today_at(n, hours).getUnixTime();
    //         return DateTime{today_epoch - 24 * 60 * 60};
    //     };
    //     auto tomorrow_at = [today_at](const DateTime& n, const time_h& hours) {
    //         auto today_epoch = today_at(n, hours).getUnixTime();
    //         return DateTime{today_epoch + 24 * 60 * 60};
    //     };
    //
    //     assert(_clock.is_ready());
    //     const auto now_dt = _clock.now();
    //     // const auto now_dt = DateTime();
    //
    //     const auto duration = _cfg.duration_hours;
    //     const auto nowtime_s = time_s(now_dt.getUnixTime());
    //
    //     // corner case: start time rolls around the 00:00 mark
    //     auto between_days = time_h(_cfg.starting_hour) + _cfg.duration_hours >= time_h(24);
    //     auto get_starttime_s = [today_at, yesterday_at](const DateTime& now_dt, bool between_days,
    //                                                     const time_h& starting_hour, const time_h& duration_hours) {
    //         if (between_days && time_h(now_dt.getHour()) < starting_hour + duration_hours - time_h(24)) {
    //             // |0    | 24| |  |
    //             // start ^ now ^  ^ end
    //             return time_s{yesterday_at(now_dt, starting_hour).getUnixTime()};
    //         }
    //         return time_s{today_at(now_dt, starting_hour).getUnixTime()};
    //     };
    //
    //     const auto starttime_s = get_starttime_s(now_dt, between_days, _cfg.starting_hour, _cfg.duration_hours);
    //     const auto endtime_s = time_s(starttime_s + duration);
    //     if (nowtime_s >= starttime_s && nowtime_s < endtime_s) {
    //         // now is ON time
    //         DBG("Growlights: now is ON time, schedule TurnOn's and LightCycleEnd!");
    //         evsys()->schedule(evsys()->event(Events::BroadSpectrumTurnOn, time_s(10)));
    //         evsys()->schedule(evsys()->event(Events::VioletSpectrumTurnOn, time_s(15)));
    //         evsys()->schedule(evsys()->event(Events::LightCycleEnd, endtime_s - nowtime_s));
    //         return;
    //     }
    //     if (nowtime_s < starttime_s) {
    //         assert(nowtime_s < endtime_s);
    //         DBG("Growlights: now is OFF time, scheduling LightCycleStart at %i:00 in %u minutes.",
    //              DateTime(starttime_s.raw()).getHour(), time_m(starttime_s - nowtime_s).printable());
    //         evsys()->schedule(evsys()->event(Events::LightCycleStart, starttime_s - nowtime_s));
    //         return;
    //     }
    //     const auto starttime_tomorrow_s = starttime_s + time_d(1);
    //     const auto time_until_start = starttime_tomorrow_s - nowtime_s;
    //     if (nowtime_s < starttime_tomorrow_s) {
    //         DBG("Growlights: now is OFF time, scheduling LightCycleStart for tomorrow at %i:00 in %u hours.",
    //              DateTime(starttime_tomorrow_s.raw()).getHour(), time_h(time_until_start).printable());
    //         evsys()->schedule(evsys()->event(Events::LightCycleStart, time_until_start));
    //         return;
    //     }
    //     assert(!"Growlights: deploy_lightcycle has not send out lightcycle events");
    // }

private:
    const Config _cfg;

    io::DigitalActuator _redblue_spectrum;
    Schedule _redblue_spectrum_schedule;

    io::DigitalActuator _full_spectrum;
    Schedule _full_spectrum_schedule;

    io::Clock _clock;
};

} // namespace kaskas::component
