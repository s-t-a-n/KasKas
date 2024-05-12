#pragma once

#include "kaskas/component.hpp"
#include "kaskas/events.hpp"
#include "kaskas/io/relay.hpp"

#include <spine/eventsystem/eventsystem.hpp>

using spn::eventsystem::EventHandler;

class Ventilation final : public Component {
public:
    struct Config {
        AnalogueOutput::Config fan_pwm_cfg;
        AnalogueOutput::Config heater_pwm_cfg;
        Relay::Config airconditioning_power_cfg;
        time_m on_time;
        time_h off_time;
    };

public:
    explicit Ventilation(Config& cfg) : Ventilation(nullptr, cfg) {}
    Ventilation(EventSystem* evsys, Config& cfg)
        : Component(evsys), _cfg(cfg), _fan(std::move(cfg.fan_pwm_cfg)), _heater(std::move(cfg.heater_pwm_cfg)),
          _power(std::move(cfg.airconditioning_power_cfg)){};

    void initialize() override {
        //
        _fan.initialize();
        _heater.initialize();
        _power.initialize();

        assert(evsys());
        evsys()->attach(Events::VentilationStart, this);
        evsys()->attach(Events::VentilationStop, this);

        const auto time_from_now = time_s(20);
        DBGF("Ventilation: Scheduling VentilationStart in %u seconds.", time_from_now.raw<unsigned>());
        evsys()->schedule(evsys()->event(Events::VentilationStart, time_from_now, Event::Data()));
    }

    void safe_shutdown(State state) override {
        DBGF("Ventilation: safe shutdown");
        // fade instead of cut to minimalize surges
        _fan.fade_to(0.0);
        _heater.fade_to(0.0);
        _power.set_state(LogicalState::OFF);
    }

    void handle_event(const Event& event) override {
        //
        switch (static_cast<Events>(event.id())) {
        case Events::VentilationStart: {
            //
            DBG("Ventilation: Started.");
            _power.set_state(LogicalState::ON);
            _fan.set_value(1.0);

            // const auto time_from_now = time_m(20);
            const auto time_from_now = _cfg.on_time;
            DBGF("Ventilation: Scheduling VentilationStop in %u minutes.", time_m(time_from_now).raw<unsigned>());
            evsys()->schedule(evsys()->event(Events::VentilationStop, time_from_now, Event::Data()));
            break;
        }
        case Events::VentilationStop: {
            //
            DBG("Ventilation: Stopped.");
            _fan.set_value(0.0);
            _power.set_state(LogicalState::OFF);

            // const auto time_from_now = time_m(20);
            const auto time_from_now = _cfg.off_time;
            DBGF("Ventilation: Scheduling VentilationStart in %u minutes.", time_m(time_from_now).raw<unsigned>());
            evsys()->schedule(evsys()->event(Events::VentilationStart, time_from_now, Event::Data()));
            break;
        }
        default: assert(!"Event was not handled!"); break;
        };
    }

private:
    const Config _cfg;

    AnalogueOutput _fan;
    AnalogueOutput _heater;
    Relay _power;
};