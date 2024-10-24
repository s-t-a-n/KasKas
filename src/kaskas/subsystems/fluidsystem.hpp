#pragma once

#include "kaskas/component.hpp"
#include "kaskas/events.hpp"
#include "kaskas/io/peripherals/relay.hpp"
#include "kaskas/io/providers/clock.hpp"
#include "kaskas/io/providers/pump.hpp"

#include <spine/core/exception.hpp>
#include <spine/core/logging.hpp>
#include <spine/core/utils/string.hpp>
#include <spine/eventsystem/eventsystem.hpp>
#include <spine/filter/implementations/ewma.hpp>
#include <spine/platform/hal.hpp>
#include <spine/structure/time/timers.hpp>

#include <cstdint>

namespace kaskas::component {

class Fluidsystem final : public Component {
public:
    using Pump = kaskas::io::Pump;
    using Event = spn::eventsystem::Event;

    struct Config {
        static constexpr std::string_view name = "Fluids";

        Pump::Config pump_cfg;
        io::HardwareStack::Idx ground_moisture_sensor_idx;
        io::HardwareStack::Idx clock_idx;

        float ground_moisture_target;

        uint16_t calibration_dosis_ml;
        uint16_t max_dosis_ml;
        k_time_s time_of_injection;
        k_time_s delay_before_effect_evaluation;
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

    Fluidsystem(io::HardwareStack& hws, EventSystem* evsys, Config& cfg)
        : Component(evsys, hws), //
          _cfg(cfg), //
          _clock(_hws.clock(_cfg.clock_idx)),
          _ground_moisture_sensor(_hws.analog_sensor(_cfg.ground_moisture_sensor_idx)), //
          _pump(_hws, std::move(cfg.pump_cfg)), _ml_per_percent_of_moisture(EWMA::Config{.K = 10}){};

    void initialize() override {
        _pump.initialize();
        _ml_per_percent_of_moisture.reset_to(0);

        spn_assert(evsys());
        evsys()->attach(Events::OutOfWater, this);
        evsys()->attach(Events::WaterInjectCheck, this);
        evsys()->attach(Events::WaterInjectEvaluateEffect, this);
        evsys()->attach(Events::WaterInjectStart, this);
        evsys()->attach(Events::WaterInjectFollowUp, this);
        evsys()->attach(Events::WaterInjectStop, this);

        const auto time_until_next_dosis = _clock.time_until_next_occurence(_cfg.time_of_injection);

        DBG("Fluidsystem: scheduling WaterInjectCheck event in %li hours (%li minutes).",
            k_time_h(time_until_next_dosis).raw(), k_time_m(time_until_next_dosis).raw());
        evsys()->schedule(Events::WaterInjectCheck, time_until_next_dosis);
    }

    void safe_shutdown(State state) override {
        DBG("Fluidsystem: Shutting down");
        _pump.stop_injection();
    }

    void handle_event(const Event& event) override {
        switch (static_cast<Events>(event.id())) {
        case Events::OutOfWater: {
            LOG("Fluidsystem: Events::OutOfWater: Pump reports it is out of water");
            return;
        }
        case Events::WaterInjectCheck: { // should injection take place?
            DBG("Fluidsystem: WaterInjectCheck: Moisture level at %.2f (threshold at %.2f, time since last injection: "
                "%li s)",
                _ground_moisture_sensor.value(), _cfg.ground_moisture_target,
                k_time_s(_pump.time_since_last_injection()).raw());

            const auto error = _cfg.ground_moisture_target - _ground_moisture_sensor.value();
            if (error <= 0) // do nothing when soil is moist enough
                return;

            auto target_amount = _ml_per_percent_of_moisture.value() * error;
            if (_ml_per_percent_of_moisture.value() == 0) {
                LOG("Fluidsystem: WaterInjectCheck: First injection since startup; injecting calibration dosis to "
                    "calibrate.");
                target_amount = _cfg.calibration_dosis_ml;
            }

            if (target_amount > _cfg.max_dosis_ml) {
                DBG("Fluidsystem: WaterInjectCheck: target amount of %.0f mL exceeds max dosis of %uf, clamping "
                    "target amount.",
                    target_amount, _cfg.max_dosis_ml);
                target_amount = _cfg.max_dosis_ml;
            }
            evsys()->schedule(Events::WaterInjectStart, k_time_s(1), Event::Data(target_amount));

            const auto time_until_next_check = _clock.time_until_next_occurence(_cfg.time_of_injection);
            DBG("Fluidsystem: WaterInjectCheck: scheduling next check in %li hours",
                k_time_h(time_until_next_check).raw())
            evsys()->schedule(Events::WaterInjectCheck, time_until_next_check);
            return;
        }
        case Events::WaterInjectEvaluateEffect: {
            spn_expect(_status.Flags.injection_needs_evaluation);
            if (_status.Flags.injection_needs_evaluation) return;

            const auto new_moisture_level = _ground_moisture_sensor.value();
            const auto effect = new_moisture_level - _moisture_level_before_injection;

            if (effect <= 0) {
                WARN(
                    "Fluidsystem: WaterInjectEvaluateEffect: injection of %u mL has negligible effect %.2f, discarding "
                    "result for evaluation.",
                    _pump.ml_since_injection_start(), effect)
                _status.Flags.injection_needs_evaluation = false;
                return;
            }

            spn_assert(effect != 0);
            const auto measured_response = _pump.ml_since_injection_start() / effect;
            LOG("Fluidsystem: WaterInjectEvaluateEffect: moisture level before injection: %.2f, after: %.2f, gives "
                "%.2f mL / %% of moisture (was: %.2f)",
                _moisture_level_before_injection, new_moisture_level, measured_response,
                _ml_per_percent_of_moisture.value());
            _ml_per_percent_of_moisture.new_sample(measured_response);
            _status.Flags.injection_needs_evaluation = false;
            return;
        }
        case Events::WaterInjectStart: {
            if (_status.Flags.injection_needs_evaluation) {
                ERR("Fluidsystem: Tried to start injection before last injection was evaluated");
                return;
            }
            if (event.data().value() <= 0) {
                ERR("Fluidsystem: Tried to start injection with a target amount of 0 mL");
                return;
            }
            LOG("Fluidsystem: Events::WaterInjectStart: injecting %.2f mL", event.data().value());
            _moisture_level_before_injection = _ground_moisture_sensor.value();
            _pump.start_injection(std::floor(event.data().value()));
            evsys()->schedule(Events::WaterInjectFollowUp, _cfg.pump_cfg.reading_interval);
            return;
        }
        case Events::WaterInjectFollowUp: {
            _pump.update();
            if (_pump.is_injecting()) {
                evsys()->schedule(Events::WaterInjectFollowUp, _cfg.pump_cfg.reading_interval);
            } else {
                DBG("Fluidsystem: Stopped injection");
                evsys()->trigger(Events::WaterInjectStop);
            }
            if (_pump.is_out_of_fluid()) {
                WARN("Fluidsystem: WaterInjectFollowUp: not enough pumped within space of time: out of fluid.");
                evsys()->schedule(Events::OutOfWater, k_time_ms(100));
            }
            return;
        }
        case Events::WaterInjectStop: {
            if (_pump.is_injecting()) _pump.stop_injection();
            _injected += _pump.ml_since_injection_start();
            LOG("Fluidsystem: Pumped %i ml in %li ms (pumped in total: %u ml), evaluating effect in %li min",
                _pump.ml_since_injection_start(), _pump.time_since_injection_start().raw(), _pump.lifetime_pumped_ml(),
                k_time_m(_cfg.delay_before_effect_evaluation).raw());

            _status.Flags.injection_needs_evaluation = true;
            evsys()->schedule(Events::WaterInjectEvaluateEffect, _cfg.delay_before_effect_evaluation);
            return;
        }
        default: spn_assert(!"event not handled"); return;
        }
    }

    std::unique_ptr<prompt::RPCRecipe> rpc_recipe() override {
        using namespace prompt;
        auto model = std::make_unique<RPCRecipe>(RPCRecipe(
            _cfg.name,
            {
                RPCModel("timeSinceLastDosis",
                         [this](const OptStringView& unit_of_time) {
                             if (unit_of_time->compare("m") == 0)
                                 return RPCResult(std::to_string(k_time_m(_pump.time_since_last_injection()).raw()));
                             if (unit_of_time->compare("h") == 0)
                                 return RPCResult(std::to_string(k_time_h(_pump.time_since_last_injection()).raw()));
                             return RPCResult(std::to_string(k_time_s(_pump.time_since_last_injection()).raw()));
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
                             evsys()->schedule(Events::WaterInjectStart, k_time_s(1),
                                               Event::Data(spn::core::utils::to_float(amount_in_ml.value())));
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
                        return RPCResult(std::to_string(_ml_per_percent_of_moisture.value()));
                    },
                    "tracks amount of moisture raised per mL of fluid dosed"),
                RPCModel(
                    "calibrationDosis",
                    [this](const OptStringView& _) { return RPCResult(std::to_string(_cfg.calibration_dosis_ml)); },
                    "The calibration dosage used when no fluid effect is known"),
                RPCModel(
                    "maxDosis", [this](const OptStringView& _) { return RPCResult(std::to_string(_cfg.max_dosis_ml)); },
                    "The maximum allowed dosage."),
            }));
        return std::move(model);
    }

    void sideload_providers(io::VirtualStackFactory& ssf) override {
        ssf.hotload_provider( //
            DataProviders::SOIL_MOISTURE_SETPOINT,
            std::make_shared<io::ContinuousValue>([this]() { return this->_cfg.ground_moisture_target; }));
        ssf.hotload_provider( //
            DataProviders::FLUID_INJECTED, std::make_shared<io::ContinuousValue>([this]() {
                const float injected = this->_injected;
                this->_injected = 0; // when injected is consumed, it is lost if not captured
                return injected;
            }));
        ssf.hotload_provider( //
            DataProviders::FLUID_INJECTED_CUMULATIVE,
            std::make_shared<io::ContinuousValue>([this]() { return _pump.lifetime_pumped_ml(); }));
        ssf.hotload_provider( //
            DataProviders::FLUID_EFFECT,
            std::make_shared<io::ContinuousValue>([this]() { return _ml_per_percent_of_moisture.value(); }));
    }

private:
    using Component = kaskas::Component;
    using EWMA = spn::filter::EWMA<float>;

    using Events = kaskas::Events;
    using EventSystem = spn::core::EventSystem;

    const Config _cfg;
    Status _status;

    const io::Clock& _clock;

    io::AnalogueSensor _ground_moisture_sensor;
    Pump _pump;

    EWMA _ml_per_percent_of_moisture; // tracks amount of moisture raised per mL of fluid injected
    float _moisture_level_before_injection = 0;

    float _injected = 0; // tracks amount injected since last lookup of FLUID_INJECTED

private:
};
} // namespace kaskas::component
