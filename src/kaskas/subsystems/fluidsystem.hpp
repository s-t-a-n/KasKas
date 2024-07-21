#pragma once

#include "kaskas/component.hpp"
#include "kaskas/events.hpp"
#include "kaskas/io/peripherals/relay.hpp"
#include "kaskas/io/providers/clock.hpp"
#include "kaskas/io/providers/pump.hpp"

#include <spine/core/exception.hpp>
#include <spine/core/timers.hpp>
#include <spine/eventsystem/eventsystem.hpp>
#include <spine/filter/implementations/ewma.hpp>
#include <spine/io/sensor.hpp>
#include <spine/platform/hal.hpp>

#include <cstdint>

namespace kaskas::component {

using kaskas::Component;
using kaskas::io::Pump;
using spn::core::Exception;
using spn::core::time::Timer;
using spn::eventsystem::Event;
using spn::eventsystem::EventHandler;
using EWMA = spn::filter::EWMA<double>;

using Events = kaskas::Events;
using EventSystem = spn::core::EventSystem;

class Fluidsystem final : public Component {
public:
    struct Config {
        Pump::Config pump_cfg;
        io::HardwareStack::Idx ground_moisture_sensor_idx;
        io::HardwareStack::Idx clock_idx;

        float ground_moisture_target;

        uint16_t max_dosis_ml;
        time_s time_of_injection;
        time_s delay_before_effect_evaluation;
    };

    union Status {
        struct BitFlags {
            bool injection_needs_evaluation : 1;
            uint8_t unused : 7;
        } Flags;
        uint8_t status = 0;
    };

public:
    Fluidsystem(io::HardwareStack& hws, Config& cfg) : Fluidsystem(hws, nullptr, cfg) {}
    Fluidsystem(io::HardwareStack& hws, EventSystem* evsys, Config& cfg) :
        Component(evsys, hws), //
        _cfg(cfg), //
        _clock(_hws.clock(_cfg.clock_idx)),
        _ground_moisture_sensor(_hws.analog_sensor(_cfg.ground_moisture_sensor_idx)), //
        _pump(_hws, std::move(cfg.pump_cfg)),
        _ml_per_percent_of_moisture(EWMA::Config{.K = 10}){};

    void initialize() override {
        _pump.initialize();
        _ml_per_percent_of_moisture.reset_to(0);

        assert(evsys());
        evsys()->attach(Events::OutOfWater, this);
        evsys()->attach(Events::WaterInjectCheck, this);
        evsys()->attach(Events::WaterInjectEvaluateEffect, this);
        evsys()->attach(Events::WaterInjectStart, this);
        evsys()->attach(Events::WaterInjectFollowUp, this);
        evsys()->attach(Events::WaterInjectStop, this);

        const auto time_until_next_dosis = _clock.time_until_next_occurence(_cfg.time_of_injection);

        DBG("Fluidsystem: scheduling WaterInjectCheck event in %u hours (%u minutes).",
            time_h(time_until_next_dosis).printable(),
            time_m(time_until_next_dosis).printable());
        evsys()->schedule(evsys()->event(Events::WaterInjectCheck, time_until_next_dosis));

        // evsys()->schedule(evsys()->event(Events::WaterInjectStart, time_s(5)));
        // evsys()->schedule(evsys()->event(Events::WaterInjectCheck, time_s(30)));
    }

    void safe_shutdown(State state) override { _pump.stop_injection(); }

    void handle_event(const Event& event) override {
        switch (static_cast<Events>(event.id())) {
        case Events::OutOfWater: {
            //
            LOG("Fluidsystem: Events::OutOfWater: Pump reports it is out of water");
            break;
        }
        case Events::WaterInjectCheck: {
            // should injection take place?
            // DBG("Fluidsystem: WaterInjectCheck");
            DBG("Fluidsystem: WaterInjectCheck: Moisture level at %.2f (threshold at %.2f, time since last injection: "
                "%u s)",
                _ground_moisture_sensor.value(),
                _cfg.ground_moisture_target,
                time_s(_pump.time_since_last_injection()).printable());

            const auto error = _cfg.ground_moisture_target - _ground_moisture_sensor.value();
            if (error <= 0) // do nothing when soil is moist enough
                return;

            auto target_amount = _ml_per_percent_of_moisture.value() * error;
            if (_ml_per_percent_of_moisture.value() == 0) {
                LOG("Fluidsystem: WaterInjectCheck: First injection since startup; injecting maximum dosis to "
                    "calibrate.");
                target_amount = _cfg.max_dosis_ml;
            }

            if (target_amount > _cfg.max_dosis_ml) {
                DBG("Fluidsystem: WaterInjectCheck: target amount of %.0f mL exceeds max dosis of %uf, clamping "
                    "target amount.",
                    target_amount,
                    _cfg.max_dosis_ml);
                target_amount = _cfg.max_dosis_ml;
            }
            evsys()->schedule(evsys()->event(Events::WaterInjectStart, time_s(1), Event::Data(target_amount)));

            const auto time_until_next_check = _clock.time_until_next_occurence(_cfg.time_of_injection);
            DBG("Fluidsystem: WaterInjectCheck: scheduling next check in %u hours",
                time_h(time_until_next_check).printable())
            evsys()->schedule(evsys()->event(Events::WaterInjectCheck, time_until_next_check));
            break;
        }
        case Events::WaterInjectEvaluateEffect: {
            assert(_status.Flags.injection_needs_evaluation);
            const auto new_moisture_level = _ground_moisture_sensor.value();
            const auto effect = new_moisture_level - _moisture_level_before_injection;
            assert(effect > 0 && "Fluidlevel dropped where it should have risen!");

            const auto measured_response = _pump.ml_since_injection_start() / effect;
            LOG("Fluidsystem: WaterInjectEvaluateEffect: moisture level before injection: %.2f, after: %.2f, gives "
                "%.2f mL / %% of moisture (was: %.2f)",
                _moisture_level_before_injection,
                new_moisture_level,
                measured_response,
                _ml_per_percent_of_moisture.value());
            _ml_per_percent_of_moisture.new_sample(measured_response);
            _status.Flags.injection_needs_evaluation = false;
            break;
        }
        case Events::WaterInjectStart: {
            if (_status.Flags.injection_needs_evaluation)
                throw_exception(spn::runtime_exception("Tried to start injection before last injection was evaluated"));
            assert(event.data().value() > 0);
            LOG("Fluidsystem: Events::WaterInjectStart: injecting %.2f mL", event.data().value());
            _moisture_level_before_injection = _ground_moisture_sensor.value();
            _pump.start_injection(std::floor(event.data().value()));
            evsys()->schedule(evsys()->event(Events::WaterInjectFollowUp, _cfg.pump_cfg.reading_interval));
            break;
        }
        case Events::WaterInjectFollowUp: {
            _pump.update();
            if (_pump.is_injecting()) {
                evsys()->schedule(evsys()->event(Events::WaterInjectFollowUp, _cfg.pump_cfg.reading_interval));
            } else {
                DBG("Fluidsystem: Stopped injection");
                evsys()->trigger(evsys()->event(Events::WaterInjectStop, time_ms(0)));
            }
            if (_pump.is_out_of_fluid()) {
                DBG("Fluidsystem: WaterInjectFollowUp: not enough pumped within space of time: out of fluid.");
                evsys()->schedule(evsys()->event(Events::OutOfWater, time_ms(100)));
            }
            break;
        }
        case Events::WaterInjectStop: {
            if (_pump.is_injecting())
                _pump.stop_injection();
            DBG("Fluidsystem: WaterInjectStop: Pumped %i ml in %i ms (pumped in total: %u ml), evaluating effect in "
                "%u min",
                _pump.ml_since_injection_start(),
                _pump.time_since_injection_start().printable(),
                _pump.lifetime_pumped_ml(),
                time_m(_cfg.delay_before_effect_evaluation).printable());
            _status.Flags.injection_needs_evaluation = true;
            evsys()->schedule(evsys()->event(Events::WaterInjectEvaluateEffect, _cfg.delay_before_effect_evaluation));
            break;
        }
        default: assert(!"event not handled"); break;
        }
    }

    std::unique_ptr<prompt::RPCRecipe> rpc_recipe() override {
        using namespace prompt;
        auto model = std::make_unique<RPCRecipe>(RPCRecipe(
            "FLU", //
            {
                RPCModel(
                    "timeSinceLastDosis",
                    [this](const OptStringView& unit_of_time) {
                        if (unit_of_time->compare("m") == 0)
                            return RPCResult(std::to_string(time_m(_pump.time_since_last_injection()).printable()));
                        if (unit_of_time->compare("h") == 0)
                            return RPCResult(std::to_string(time_h(_pump.time_since_last_injection()).printable()));
                        return RPCResult(std::to_string(time_s(_pump.time_since_last_injection()).printable()));
                    }),
                RPCModel("isOutOfWater",
                         [this](const OptStringView&) {
                             return RPCResult(this->_pump.is_out_of_fluid() ? "True" : "False"); // Pythonic boolean
                         }),
                RPCModel("waterNow",
                         [this](const OptStringView& amount_in_ml) {
                             if (!amount_in_ml)
                                 return RPCResult("cannot inject: amount to inject (in ml) not specified",
                                                  RPCResult::Status::BAD_INPUT);
                             if (_status.Flags.injection_needs_evaluation)
                                 return RPCResult("cannot inject: last injection was not evaluated",
                                                  RPCResult::Status::BAD_RESULT);
                             evsys()->schedule(evsys()->event(Events::WaterInjectStart,
                                                              time_s(1),
                                                              Event::Data(std::stod(*amount_in_ml))));
                             return RPCResult(RPCResult::Status::OK);
                         }),
                RPCModel("resetEvaluationLock",
                         [this](const OptStringView& amount_in_ml) {
                             _status.Flags.injection_needs_evaluation = false;
                             return RPCResult(RPCResult::Status::OK);
                         }),
                RPCModel(
                    "injectionEffect",
                    [this](const OptStringView& _) {
                        return RPCResult(std::to_string(_ml_per_percent_of_moisture.value()), RPCResult::Status::OK);
                    },
                    "tracks amount of moisture raised per mL of fluid dosed"),
            }));
        return std::move(model);
    }
    void sideload_providers(io::VirtualStackFactory& ssf) override {
        ssf.hotload_provider(DataProviders::SOIL_MOISTURE_SETPOINT, std::make_shared<io::ContinuousValue>([this]() {
                                 return this->_cfg.ground_moisture_target;
                             }));
    }

private:
    const Config _cfg;
    Status _status;

    const io::Clock& _clock;

    io::AnalogueSensor _ground_moisture_sensor;
    Pump _pump;

    EWMA _ml_per_percent_of_moisture; // tracks amount of moisture raised per mL of fluid injected
    double _moisture_level_before_injection = 0;

private:
};

} // namespace kaskas::component
