#pragma once

#include "kaskas/component.hpp"
#include "kaskas/events.hpp"

#include <spine/core/exception.hpp>
#include <spine/eventsystem/eventsystem.hpp>

namespace kaskas::component {

using spn::core::Event;
using spn::eventsystem::EventHandler;
/// Responsible for maintaining hardware
class Hardware : public Component {
public:
    struct Config {};

public:
    Hardware(io::HardwareStack& hws, const Config& cfg) : Hardware(hws, nullptr, cfg) {}
    Hardware(io::HardwareStack& hws, EventSystem* evsys, const Config& cfg) : Component(evsys, hws), _cfg(cfg){};

    void initialize() override {
        assert(evsys());
        evsys()->attach(Events::SensorFollowUp, this);
        evsys()->attach(Events::ShutDown, this);

        evsys()->schedule(evsys()->event(Events::SensorFollowUp, _hws.time_until_next_update()));
    }

    void safe_shutdown(State state) override {
        DBG("Hardware: Safeshutdown");
        _hws.safe_shutdown(state == State::CRITICAL);
    }

    void handle_event(const Event& event) override {
        switch (static_cast<Events>(event.id())) {
        case Events::SensorFollowUp:
            _hws.update_all();
            evsys()->schedule(evsys()->event(Events::SensorFollowUp, _hws.time_until_next_update()));
            break;
        case Events::ShutDown: spn::throw_exception(spn::runtime_exception("Scheduled shutdown.")); break;
        default: assert(!"Event was not handled!"); break;
        };
    }

    std::unique_ptr<prompt::RPCRecipe> rpc_recipe() override {
        using namespace prompt;
        auto model = std::make_unique<RPCRecipe>(
            RPCRecipe("HW", //
                      {
                          RPCModel("shutdown",
                                   [this](const OptStringView& _) {
                                       evsys()->schedule(evsys()->event(Events::ShutDown, time_s(1)));
                                       return RPCResult(RPCResult::Status::OK);
                                   }),
                      }));
        return std::move(model);
    }
    void sideload_providers(io::VirtualStackFactory& ssf) override {}

private:
    const Config _cfg;
};

} // namespace kaskas::component
