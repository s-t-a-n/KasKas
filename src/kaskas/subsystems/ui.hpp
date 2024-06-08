#pragma once

#include "kaskas/component.hpp"
#include "kaskas/events.hpp"
#include "kaskas/io/providers/signaltower.hpp"

#include <spine/core/exception.hpp>
#include <spine/eventsystem/eventsystem.hpp>

namespace kaskas::component {

using spn::eventsystem::EventHandler;

class UI : public Component {
public:
    struct Config {
        Signaltower::Config signaltower_cfg;
        DigitalInput::Config userbutton_cfg;
        time_ms watchdog_interval = time_s(1);
    };

public:
    UI(io::HardwareStack& hws, const Config& cfg) : UI(hws, nullptr, cfg) {}
    UI(io::HardwareStack& hws, EventSystem* evsys, const Config& cfg)
        : Component(evsys, hws), _cfg(cfg), _signaltower(cfg.signaltower_cfg),
          _userbutton(std::move(_cfg.userbutton_cfg)){};

    void initialize() override {
        //
        _signaltower.initialize();
        _userbutton.initialize();

#if defined(STM32F429xx)
        _builtin_led_blue.initialize();
        _builtin_led_green.initialize();
        _builtin_led_red.initialize();
        _builtin_led_green.set_state(LogicalState::ON);
#endif

        assert(evsys());
        evsys()->attach(Events::WakeUp, this);
        evsys()->attach(Events::UIButtonCheck, this);
        evsys()->attach(Events::UIWatchDog, this);
        evsys()->attach(Events::OutOfWater, this);

        evsys()->schedule(evsys()->event(Events::UIButtonCheck, time_s(1), Event::Data()));
        evsys()->schedule(evsys()->event(Events::UIWatchDog, _cfg.watchdog_interval, Event::Data()));
    }

    void safe_shutdown(State state) override {
        DBGF("UI: Safeshutdown");
        switch (state) {
        case State::SAFE: _signaltower.signal(Signaltower::State::Yellow); break;
        case State::CRITICAL: _signaltower.signal(Signaltower::State::Red); break;
        default: break;
        }
#if defined(STM32F429xx)
        _builtin_led_red.set_state(LogicalState::OFF);
        _builtin_led_blue.set_state(LogicalState::OFF);
        _builtin_led_green.set_state(LogicalState::OFF);
#endif
    }

    void handle_event(const Event& event) override {
        //
        switch (static_cast<Events>(event.id())) {
        case Events::WakeUp: {
            //
            DBG("UI: WakeUp");
            _signaltower.signal(Signaltower::State::Green);
            break;
        }
        case Events::UIButtonCheck: {
            if (_userbutton.state() == LogicalState::ON) {
                dbg::throw_exception(spn::core::runtime_error("Monkey pressed button. Shut down gracefully."));
            }
            evsys()->schedule(evsys()->event(Events::UIButtonCheck, time_s(1), Event::Data()));
            break;
        }
        case Events::UIWatchDog: {
#if defined(STM32F429xx)
            _builtin_led_green.flip();
            // _builtin_led_red.flip();
            // _builtin_led_blue.flip();
#endif

            evsys()->schedule(evsys()->event(Events::UIWatchDog, _cfg.watchdog_interval, Event::Data()));
            break;
        }
        case Events::OutOfWater: {
            //
            DBG("UI: OutOfWater");
            _signaltower.signal(Signaltower::State::Yellow);
            break;
        }
        default: assert(!"Event was not handled!"); break;
        };
    }

private:
    const Config _cfg;

    Signaltower _signaltower;

    DigitalInput _userbutton;

#if defined(STM32F429xx)
    DigitalOutput _builtin_led_blue = DigitalOutput::Config{.pin = PB7, .active_on_low = false};
    DigitalOutput _builtin_led_red = DigitalOutput::Config{.pin = PB14, .active_on_low = false};
    DigitalOutput _builtin_led_green = DigitalOutput::Config{.pin = PB0, .active_on_low = false};
#endif
};

} // namespace kaskas::component