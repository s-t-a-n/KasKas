#pragma once

#include "kaskas/component.hpp"
#include "kaskas/events.hpp"

#include <spine/core/exception.hpp>
#include <spine/eventsystem/eventsystem.hpp>

namespace kaskas::component {

using spn::core::Event;
using spn::eventsystem::EventHandler;
// Responsible for maintaining hardware
class Hardware : public Component {
public:
    struct Config {};

public:
    Hardware(io::HardwareStack& hws, const Config& cfg) : Hardware(hws, nullptr, cfg) {}
    Hardware(io::HardwareStack& hws, EventSystem* evsys, const Config& cfg) : Component(evsys, hws), _cfg(cfg){};

    void initialize() override {
        assert(evsys());
        evsys()->attach(Events::SensorFollowUp, this);

        evsys()->schedule(evsys()->event(Events::SensorFollowUp, _hws.time_until_next_update(), Event::Data()));
    }

    void safe_shutdown(State state) override {
        DBGF("Hardware: Safeshutdown");
        _hws.safe_shutdown(state == State::CRITICAL);
    }

    void handle_event(const Event& event) override {
        switch (static_cast<Events>(event.id())) {
        case Events::SensorFollowUp: {
            // DBG("Hardware: SensorFollowUp");
            _hws.update_all();
            evsys()->schedule(evsys()->event(Events::SensorFollowUp, _hws.time_until_next_update(), Event::Data()));
            break;
        }
        default: assert(!"Event was not handled!"); break;
        };
    }

    std::unique_ptr<prompt::RPCRecipe> rpc_recipe() override { return {}; }
    void sideload_providers(io::VirtualStackFactory& ssf) override {}

private:
    const Config _cfg;
};

} // namespace kaskas::component