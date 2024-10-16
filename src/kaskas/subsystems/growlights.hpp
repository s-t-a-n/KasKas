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

class Growlights final : public Component {
public:
    using Event = spn::eventsystem::Event;
    using Schedule = spn::structure::time::Schedule;

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
        : Component(evsys, hws), _cfg(cfg), _violet_spectrum(_hws.digital_actuator(_cfg.redblue_spectrum_actuator_idx)),
          _violet_spectrum_schedule(std::move(_cfg.redblue_spectrum_schedule)),
          _broad_spectrum(_hws.digital_actuator(_cfg.full_spectrum_actuator_idx)),
          _broad_spectrum_schedule(std::move(_cfg.full_spectrum_schedule)), _clock(_hws.clock(_cfg.clock_idx)){};

    void initialize() override {
        spn_assert(evsys());
        evsys()->attach(Events::LightBroadSpectrumCycleCheck, this);
        evsys()->attach(Events::LightBroadSpectrumTurnOn, this);
        evsys()->attach(Events::LightBroadSpectrumTurnOff, this);
        evsys()->attach(Events::LightVioletSpectrumCycleCheck, this);
        evsys()->attach(Events::LightVioletSpectrumTurnOn, this);
        evsys()->attach(Events::LightVioletSpectrumTurnOff, this);

        auto time_from_now = k_time_s(5);
        DBG("Growlights: Scheduling check for broad spectrum lights in %li seconds.", time_from_now.raw());
        evsys()->schedule(Events::LightBroadSpectrumCycleCheck, time_from_now);
        time_from_now += k_time_s(5);
        DBG("Growlights: Scheduling check for violet specturm lights in %li seconds.", time_from_now.raw());
        evsys()->schedule(Events::LightVioletSpectrumCycleCheck, time_from_now);
    }

    void safe_shutdown(State state) override {
        DBG("Growlights: Safeshutdown");
        HAL::delay(k_time_ms(300));
        _violet_spectrum.set_state(LogicalState::OFF);
        HAL::delay(k_time_ms(300));
        _broad_spectrum.set_state(LogicalState::OFF);
    }

    void handle_event(const Event& event) override {
        const auto now_s = [&]() {
            if (!_clock.is_ready()) {
                spn::throw_exception(
                    spn::runtime_exception("ClimateControl: Cannot schedule because of clock failure"));
            }
            const auto now_dt = _clock.now();
            return k_time_s(k_time_h(now_dt.getHour())) + k_time_m(now_dt.getMinute());
        };
        switch (static_cast<Events>(event.id())) {
        case Events::LightBroadSpectrumCycleCheck: {
            const auto now = now_s();
            const auto next_setpoint = _broad_spectrum_schedule.value_at(now);

            if (next_setpoint > 0 && _broad_spectrum.value() == 0) {
                LOG("Growlights: Broad spectrum lights are currently off, turning ON");
                evsys()->trigger(Events::LightBroadSpectrumTurnOn);
            } else if (next_setpoint == 0 && _broad_spectrum.value() > 0) {
                LOG("Growlights: Broad spectrum lights are currently on, turning OFF");
                evsys()->trigger(Events::LightBroadSpectrumTurnOff);
            }

            const auto time_until_next_check = _broad_spectrum_schedule.start_of_next_block(now) - now;
            spn_assert(_broad_spectrum_schedule.start_of_next_block(now) > now);

            DBG("Growlights: Scheduling next check for broad spectrum lights in %li m",
                k_time_m(time_until_next_check).raw());
            spn_assert(time_until_next_check.raw<>() > 0);
            evsys()->schedule(Events::LightBroadSpectrumCycleCheck, time_until_next_check);
            break;
        }
        case Events::LightBroadSpectrumTurnOn: {
            DBG("Growlights: Turning broad spectrum lights ON.");
            _broad_spectrum.set_state(LogicalState::ON);
            break;
        }
        case Events::LightBroadSpectrumTurnOff: {
            DBG("Growlights: Turning broad spectrum lights OFF.");
            _broad_spectrum.set_state(LogicalState::OFF);
            break;
        }

        case Events::LightVioletSpectrumCycleCheck: {
            const auto now = now_s();
            const auto next_setpoint = _violet_spectrum_schedule.value_at(now);

            if (next_setpoint > 0 && _violet_spectrum.value() == 0) {
                LOG("Growlights: Violet spectrum lights are currently off, turning ON");
                evsys()->trigger(Events::LightVioletSpectrumTurnOn);
            } else if (next_setpoint == 0 && _violet_spectrum.value() > 0) {
                LOG("Growlights: Violet spectrum lights are currently on, turning OFF");
                evsys()->trigger(Events::LightVioletSpectrumTurnOff);
            }

            const auto time_until_next_check = _violet_spectrum_schedule.start_of_next_block(now) - now;
            spn_assert(_violet_spectrum_schedule.start_of_next_block(now) > now);

            DBG("Growlights: Scheduling next check for violet spectrum lights in %li m",
                k_time_m(time_until_next_check).raw());
            spn_assert(time_until_next_check.raw<>() > 0);
            evsys()->schedule(Events::LightVioletSpectrumCycleCheck, time_until_next_check);
            break;
        }
        case Events::LightVioletSpectrumTurnOn: {
            DBG("Growlights: Turning violet spectrum lights ON.");
            _violet_spectrum.set_state(LogicalState::ON);
            break;
        }
        case Events::LightVioletSpectrumTurnOff: {
            DBG("Growlights: Turning violet spectrum lights OFF.");
            _violet_spectrum.set_state(LogicalState::OFF);
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
                              "turnOnBroadSpectrumLights",
                              [this](const OptStringView& unit_of_time) {
                                  if (!unit_of_time.has_value()) {
                                      evsys()->trigger(Events::LightBroadSpectrumTurnOn);
                                      return RPCResult(RPCResult::Status::OK);
                                  }

                                  auto time_variant = spn::core::utils::parse_time(unit_of_time.value());
                                  if (!time_variant.has_value()) {
                                      return RPCResult("Error: Invalid time format.");
                                  }

                                  std::visit(
                                      [this](auto&& time) {
                                          evsys()->trigger(Events::LightBroadSpectrumTurnOn);
                                          evsys()->schedule(Events::LightBroadSpectrumTurnOff, time);
                                      },
                                      time_variant.value());
                                  return RPCResult(RPCResult::Status::OK);
                              },
                              "Args: length and unit of time, eg. 10s or 1d"),
                          RPCModel(
                              "turnOnVioletSpectrumLights",
                              [this](const OptStringView& unit_of_time) {
                                  if (!unit_of_time.has_value()) {
                                      evsys()->trigger(Events::LightVioletSpectrumTurnOn);
                                      return RPCResult(RPCResult::Status::OK);
                                  }

                                  auto time_variant = spn::core::utils::parse_time(unit_of_time.value());
                                  if (!time_variant.has_value()) {
                                      return RPCResult("Error: Invalid time format.");
                                  }

                                  std::visit(
                                      [this](auto&& time) {
                                          evsys()->trigger(Events::LightVioletSpectrumTurnOn);
                                          evsys()->schedule(Events::LightVioletSpectrumTurnOff, time);
                                      },
                                      time_variant.value());
                                  return RPCResult(RPCResult::Status::OK);
                              },
                              "Args: length and unit of time, eg. 10s or 1d"),
                          RPCModel("turnOffBroadSpectrumLights",
                                   [this](const OptStringView& unit_of_time) {
                                       if (!unit_of_time.has_value()) {
                                           evsys()->trigger(Events::LightBroadSpectrumTurnOff);
                                           return RPCResult(RPCResult::Status::OK);
                                       }
                                       return RPCResult("no arguments accepted", RPCResult::Status::BAD_INPUT);
                                   }),
                          RPCModel("turnOffVioletSpectrumLights",
                                   [this](const OptStringView& unit_of_time) {
                                       if (!unit_of_time.has_value()) {
                                           evsys()->trigger(Events::LightVioletSpectrumTurnOff);
                                           return RPCResult(RPCResult::Status::OK);
                                       }
                                       return RPCResult("no arguments accepted", RPCResult::Status::BAD_INPUT);
                                   }),
                      }));
        return std::move(model);
    }
    void sideload_providers(io::VirtualStackFactory& ssf) override {}

private:
    using Events = kaskas::Events;
    using EventSystem = spn::core::EventSystem;
    using Component = kaskas::Component;

    using LogicalState = spn::core::LogicalState;

    const Config _cfg;

    io::DigitalActuator _violet_spectrum;
    Schedule _violet_spectrum_schedule;

    io::DigitalActuator _broad_spectrum;
    Schedule _broad_spectrum_schedule;

    io::Clock _clock;
};

} // namespace kaskas::component
