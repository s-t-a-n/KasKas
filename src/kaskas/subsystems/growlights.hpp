#pragma once

#include "kaskas/component.hpp"
#include "kaskas/events.hpp"
#include "kaskas/io/hardware_stack.hpp"
#include "kaskas/io/providers/digital.hpp"

#include <kaskas/component.hpp>
#include <spine/core/exception.hpp>
#include <spine/core/types.hpp>
#include <spine/core/utils/time_repr.hpp>
#include <spine/eventsystem/eventsystem.hpp>
#include <spine/structure/time/schedule.hpp>
#include <spine/structure/time/timers.hpp>

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
    Growlights(io::HardwareStack& hws, EventSystem* evsys, const Config& cfg)
        : Component(evsys, hws), _cfg(cfg),
          _redblue_spectrum(_hws.digital_actuator(_cfg.redblue_spectrum_actuator_idx)),
          _redblue_spectrum_schedule(std::move(_cfg.redblue_spectrum_schedule)),
          _full_spectrum(_hws.digital_actuator(_cfg.full_spectrum_actuator_idx)),
          _full_spectrum_schedule(std::move(_cfg.full_spectrum_schedule)), _clock(_hws.clock(_cfg.clock_idx)){};

    void initialize() override {
        spn_assert(evsys());
        evsys()->attach(Events::LightFullSpectrumCycleCheck, this);
        evsys()->attach(Events::LightFullSpectrumTurnOn, this);
        evsys()->attach(Events::LightFullSpectrumTurnOff, this);
        evsys()->attach(Events::LightRedBlueSpectrumCycleCheck, this);
        evsys()->attach(Events::LightRedBlueSpectrumTurnOn, this);
        evsys()->attach(Events::LightRedBlueSpectrumTurnOff, this);

        auto time_from_now = time_s(5);
        DBG("Growlights: Scheduling LightFullSpectrumCycleCheck in %u seconds.", time_from_now.printable());
        evsys()->schedule(Events::LightFullSpectrumCycleCheck, time_from_now);
        time_from_now += time_s(5);
        DBG("Growlights: Scheduling LightRedBlueSpectrumCycleCheck in %u seconds.", time_from_now.printable());
        evsys()->schedule(Events::LightRedBlueSpectrumCycleCheck, time_from_now);
    }

    void safe_shutdown(State state) override {
        HAL::delay(time_ms(300));
        _redblue_spectrum.set_state(LogicalState::OFF);
        HAL::delay(time_ms(300));
        _full_spectrum.set_state(LogicalState::OFF);
    }

    void handle_event(const Event& event) override {
        const auto now_s = [&]() {
            spn_assert(_clock.is_ready());
            const auto now_dt = _clock.now();
            return time_s(time_h(now_dt.getHour())) + time_m(now_dt.getMinute());
        };
        switch (static_cast<Events>(event.id())) {
        case Events::LightFullSpectrumCycleCheck: {
            const auto now = now_s();
            const auto next_setpoint = _full_spectrum_schedule.value_at(now);

            if (next_setpoint > 0 && _full_spectrum.value() == 0) {
                DBG("Growlights: Check: full spectrum is currently off, turn on");
                evsys()->trigger(Events::LightFullSpectrumTurnOn);
            } else if (next_setpoint == 0 && _full_spectrum.value() > 0) {
                DBG("Growlights: Check: full spectrum is currently on, turn off");
                evsys()->trigger(Events::LightFullSpectrumTurnOff);
            }

            const auto time_until_next_check = _full_spectrum_schedule.start_of_next_block(now) - now;
            spn_assert(_full_spectrum_schedule.start_of_next_block(now) > now);

            DBG("Growlights: Checked. Scheduling next check in %u m", time_m(time_until_next_check).printable());
            spn_assert(time_until_next_check.raw<>() > 0);
            evsys()->schedule(Events::LightFullSpectrumCycleCheck, time_until_next_check);
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
                evsys()->trigger(Events::LightRedBlueSpectrumTurnOn);
            } else if (next_setpoint == 0 && _redblue_spectrum.value() > 0) {
                DBG("Growlights: Check: red/blue spectrum is currently on, turn off");
                evsys()->trigger(Events::LightRedBlueSpectrumTurnOff);
            }

            const auto time_until_next_check = _redblue_spectrum_schedule.start_of_next_block(now) - now;
            spn_assert(_redblue_spectrum_schedule.start_of_next_block(now) > now);

            DBG("Growlights: Checked. Scheduling next check in %u m", time_m(time_until_next_check).printable());
            spn_assert(time_until_next_check.raw<>() > 0);
            evsys()->schedule(Events::LightRedBlueSpectrumCycleCheck, time_until_next_check);
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

        default: spn_assert(!"Event was not handled!"); break;
        }
    }

    std::unique_ptr<prompt::RPCRecipe> rpc_recipe() override {
        using namespace prompt;
        auto model = std::make_unique<RPCRecipe>(
            RPCRecipe("Growlights", //
                      {
                          RPCModel(
                              "turnOnFullSpectrum",
                              [this](const OptStringView& unit_of_time) {
                                  if (!unit_of_time.has_value()) {
                                      evsys()->trigger(Events::LightFullSpectrumTurnOn);
                                      return RPCResult(RPCResult::Status::OK);
                                  }

                                  auto time_variant = spn::core::utils::parse_time(unit_of_time.value());
                                  if (!time_variant.has_value()) {
                                      return RPCResult("Error: Invalid time format.");
                                  }

                                  std::visit(
                                      [this](auto&& time) {
                                          evsys()->trigger(Events::LightFullSpectrumTurnOn);
                                          evsys()->schedule(Events::LightFullSpectrumTurnOff, time);
                                      },
                                      time_variant.value());
                                  return RPCResult(RPCResult::Status::OK);
                              },
                              "Args: length and unit of time, eg. 10s or 1d"),
                          RPCModel(
                              "turnOnRedBlueSpectrum",
                              [this](const OptStringView& unit_of_time) {
                                  if (!unit_of_time.has_value()) {
                                      evsys()->trigger(Events::LightRedBlueSpectrumTurnOn);
                                      return RPCResult(RPCResult::Status::OK);
                                  }

                                  auto time_variant = spn::core::utils::parse_time(unit_of_time.value());
                                  if (!time_variant.has_value()) {
                                      return RPCResult("Error: Invalid time format.");
                                  }

                                  std::visit(
                                      [this](auto&& time) {
                                          evsys()->trigger(Events::LightRedBlueSpectrumTurnOn);
                                          evsys()->schedule(Events::LightRedBlueSpectrumTurnOff, time);
                                      },
                                      time_variant.value());
                                  return RPCResult(RPCResult::Status::OK);
                              },
                              "Args: length and unit of time, eg. 10s or 1d"),
                          RPCModel("turnOffFullSpectrum",
                                   [this](const OptStringView& unit_of_time) {
                                       if (!unit_of_time.has_value()) {
                                           evsys()->trigger(Events::LightFullSpectrumTurnOff);
                                           return RPCResult(RPCResult::Status::OK);
                                       }
                                       return RPCResult("no arguments accepted", RPCResult::Status::BAD_INPUT);
                                   }),
                          RPCModel("turnOffRedBlueSpectrum",
                                   [this](const OptStringView& unit_of_time) {
                                       if (!unit_of_time.has_value()) {
                                           evsys()->trigger(Events::LightRedBlueSpectrumTurnOff);
                                           return RPCResult(RPCResult::Status::OK);
                                       }
                                       return RPCResult("no arguments accepted", RPCResult::Status::BAD_INPUT);
                                   }),
                      }));
        return std::move(model);
    }
    void sideload_providers(io::VirtualStackFactory& ssf) override {}

private:
    using LogicalState = spn::core::LogicalState;

    const Config _cfg;

    io::DigitalActuator _redblue_spectrum;
    Schedule _redblue_spectrum_schedule;

    io::DigitalActuator _full_spectrum;
    Schedule _full_spectrum_schedule;

    io::Clock _clock;
};

} // namespace kaskas::component
