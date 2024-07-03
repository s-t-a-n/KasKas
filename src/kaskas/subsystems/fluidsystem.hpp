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

#include <AH/STL/cstdint>

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
    // using GroundMoistureSensorFilter = EWMA;
    // using GroundMoistureSensor = Sensor<AnalogueInput, GroundMoistureSensorFilter>;

    struct Config {
        Pump::Config pump_cfg;
        io::HardwareStack::Idx ground_moisture_sensor_idx;

        float ground_moisture_threshold; // value between 0.0 and 1.0 with 1.0 being the moistest of states the
        // sensor can pick up
        uint16_t inject_dosis_ml; // dosis in ml to inject
        time_m inject_check_interval; // interval between checking if injection is needed
    };

public:
    Fluidsystem(io::HardwareStack& hws, Config& cfg) : Fluidsystem(hws, nullptr, cfg) {}
    Fluidsystem(io::HardwareStack& hws, EventSystem* evsys, Config& cfg)
        : Component(evsys, hws), //
          _cfg(cfg), //
          _ground_moisture_sensor(_hws.analog_sensor(_cfg.ground_moisture_sensor_idx)), //
          _pump(_hws, std::move(cfg.pump_cfg)){};

    void initialize() override {
        _pump.initialize();

        assert(evsys());
        evsys()->attach(Events::OutOfWater, this);
        evsys()->attach(Events::WaterInjectCheck, this);
        evsys()->attach(Events::WaterInjectStart, this);
        evsys()->attach(Events::WaterInjectFollowUp, this);
        evsys()->attach(Events::WaterInjectStop, this);

        DBGF("Fluidsystem: scheduling WaterInjectCheck event in %u hours (%u minutes).",
             time_h(_cfg.inject_check_interval).printable(), time_m(_cfg.inject_check_interval).printable());
        evsys()->schedule(evsys()->event(Events::WaterInjectCheck, _cfg.inject_check_interval, Event::Data()));

        // evsys()->schedule(evsys()->event(Events::WaterInjectStart, time_s(5), Event::Data()));
        // evsys()->schedule(evsys()->event(Events::WaterInjectCheck, time_s(30), Event::Data()));
    }

    void safe_shutdown(State state) override { _pump.stop_injection(); }

    void handle_event(const Event& event) override {
        switch (static_cast<Events>(event.id())) {
        case Events::OutOfWater: {
            //
            DBG("Fluidsystem: OutOfWater");
            break;
        }
        case Events::WaterInjectCheck: {
            // should injection take place?
            // DBG("Fluidsystem: WaterInjectCheck");
            DBGF("Fluidsystem: WaterInjectCheck: Moisture level at %.2f (threshold at %.2f, time since last injection: "
                 "%u s)",
                 _ground_moisture_sensor.value(), _cfg.ground_moisture_threshold,
                 time_s(_pump.time_since_last_injection()).printable());
            if (_ground_moisture_sensor.value() < _cfg.ground_moisture_threshold
                && _pump.time_since_last_injection() >= _cfg.inject_check_interval) {
                DBGF("Fluidsystem: Moisture level (%.2f) crossed threshold (%.2f) and %u s since last, calling for "
                     "injection start.",
                     _ground_moisture_sensor.value(), _cfg.ground_moisture_threshold,
                     time_s(_pump.time_since_last_injection()).printable());
                evsys()->schedule(evsys()->event(Events::WaterInjectStart, time_s(1), Event::Data()));
            }
            evsys()->schedule(evsys()->event(Events::WaterInjectCheck, _cfg.inject_check_interval, Event::Data()));
            break;
        }
        case Events::WaterInjectStart: {
            DBG("Fluidsystem: WaterInjectStart");
            _pump.start_injection();
            evsys()->schedule(
                evsys()->event(Events::WaterInjectFollowUp, _cfg.pump_cfg.reading_interval, Event::Data()));
            break;
        }
        case Events::WaterInjectFollowUp: {
            //                        DBG("Fluidsystem: WaterInjectFollowUp");
            bool stop = false;
            [[maybe_unused]] const auto injected = _pump.read_ml_injected();

            if (_pump.time_since_injection_start() > _cfg.pump_cfg.pump_timeout && _pump.flowrate_lm() < 0.1) {
                // not enough pumped within space of time
                DBG("Fluidsystem: WaterInjectFollowUp: not enough pumped within space of time");
                stop = true;
                evsys()->schedule(evsys()->event(Events::OutOfWater, time_ms(100), Event::Data()));
            }
            if (_pump.ml_since_injection_start() >= _cfg.inject_dosis_ml) {
                // reached dosis
                DBG("Fluidsystem: WaterInjectFollowUp: Satisfied start_injection request");
                stop = true;
            }

            if (stop) {
                DBG("Fluidsystem: Stopping injection");
                evsys()->trigger(evsys()->event(Events::WaterInjectStop, time_ms(0), Event::Data()));
            } else {
                evsys()->schedule(
                    evsys()->event(Events::WaterInjectFollowUp, _cfg.pump_cfg.reading_interval, Event::Data()));
            }
            break;
        }
        case Events::WaterInjectStop: {
            DBG("Fluidsystem: WaterInjectStop");
            _pump.stop_injection();
            DBGF("Fluidsystem: Pumped %i ml in %i ms (pumped in total: %u ml)", _pump.ml_since_injection_start(),
                 _pump.time_since_injection_start().printable(), _pump.lifetime_pumped_ml());
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
                    "timeSinceLastWatering",
                    [this](const OptStringView& unit_of_time) {
                        if (unit_of_time == "m")
                            return RPCResult(std::to_string(time_m(_pump.time_since_last_injection()).printable()));
                        if (unit_of_time == "h")
                            return RPCResult(std::to_string(time_h(_pump.time_since_last_injection()).printable()));
                        return RPCResult(std::to_string(time_s(_pump.time_since_last_injection()).printable()));
                    }),
                RPCModel("waterNow",
                         [this](const OptStringView& amount_in_ml) {
                             evsys()->schedule(evsys()->event(Events::WaterInjectStart, time_s(1), Event::Data()));
                             return RPCResult(RPCResult::State::OK);
                         }),
            }));
        return std::move(model);
    }
    void sideload_providers(io::VirtualStackFactory& ssf) override {
        ssf.hotload_provider(DataProviders::SOIL_MOISTURE_SETPOINT, std::make_shared<io::ContinuousValue>([this]() {
                                 return this->_cfg.ground_moisture_threshold;
                             }));
    }

private:
    const Config _cfg;

    io::AnalogueSensor _ground_moisture_sensor;
    Pump _pump;
};

} // namespace kaskas::component