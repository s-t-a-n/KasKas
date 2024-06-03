#pragma once

#include "kaskas/component.hpp"
#include "kaskas/events.hpp"
#include "kaskas/io/signaltower.hpp"

#include <spine/core/exception.hpp>
#include <spine/eventsystem/eventsystem.hpp>

namespace kaskas::component {

using spn::eventsystem::EventHandler;

class UI : public Component {
public:
    struct Config {
        Signaltower::Config signaltower_cfg;
        DigitalInput::Config userbutton_cfg;
    };

public:
    explicit UI(Config& cfg) : UI(nullptr, cfg) {}
    UI(EventSystem* evsys, Config& cfg)
        : Component(evsys), _cfg(cfg), _signaltower(cfg.signaltower_cfg), _userbutton(std::move(_cfg.userbutton_cfg)){};

    void initialize() override {
        //
        _signaltower.initialize();
        _userbutton.initialize();

        assert(evsys());
        evsys()->attach(Events::WakeUp, this);
        evsys()->attach(Events::UserButtonCheck, this);
        evsys()->attach(Events::OutOfWater, this);

        evsys()->schedule(evsys()->event(Events::UserButtonCheck, time_s(1), Event::Data()));
    }

    void safe_shutdown(State state) override {
        DBGF("UI: Safeshutdown");
        switch (state) {
        case State::SAFE: _signaltower.signal(Signaltower::State::Yellow); break;
        case State::CRITICAL: _signaltower.signal(Signaltower::State::Red); break;
        default: break;
        }
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
        case Events::UserButtonCheck: {
            if (_userbutton.state() == LogicalState::ON) {
                dbg::throw_exception(spn::core::runtime_error("User pressed button. Shutting down gracefully"));
            }
            evsys()->schedule(evsys()->event(Events::UserButtonCheck, time_s(1), Event::Data()));
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
};

} // namespace kaskas::component