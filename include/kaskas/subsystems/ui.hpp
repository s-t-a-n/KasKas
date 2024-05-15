#pragma once

#include "kaskas/component.hpp"
#include "kaskas/events.hpp"
#include "kaskas/io/signaltower.hpp"

#include <spine/eventsystem/eventsystem.hpp>

namespace kaskas::component {
using spn::eventsystem::EventHandler;

class UI : public Component {
public:
    struct Config {
        Signaltower::Config signaltower_cfg;
    };

public:
    explicit UI(Config& cfg) : UI(nullptr, cfg) {}
    UI(EventSystem* evsys, Config& cfg) : Component(evsys), _cfg(cfg), _signaltower(cfg.signaltower_cfg){};

    void initialize() override {
        //
        _signaltower.initialize();

        assert(evsys());
        evsys()->attach(Events::WakeUp, this);
        evsys()->attach(Events::OutOfWater, this);
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
};

} // namespace kaskas::component