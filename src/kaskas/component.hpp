#pragma once

#include "io/software_stack.hpp"
#include "kaskas/io/hardware_stack.hpp"
#include "kaskas/prompt/prompt.hpp"

#include <spine/eventsystem/eventsystem.hpp>

namespace kaskas {

using prompt::Prompt;
using spn::core::EventHandler;
using spn::core::EventSystem;

class Component : public EventHandler {
public:
    enum class State { SAFE, CRITICAL };

public:
    explicit Component(io::HardwareStack& hws) : Component(nullptr, hws) {}
    explicit Component(EventSystem* event_system, io::HardwareStack& hws) : EventHandler(event_system), _hws(hws) {}

    virtual void initialize() { assert(!"Virtual base function called"); }
    virtual void safe_shutdown(State state = State::SAFE) { assert(!"Virtual base function called"); }

    virtual std::unique_ptr<prompt::RPCRecipe> rpc_recipe() {
        assert(!"Virtual base function called");
        return {};
    }
    virtual void sideload_providers(io::VirtualStackFactory& ssf) { assert(!"Virtual base function called"); }

protected:
    io::HardwareStack& _hws;

private:
};

} // namespace kaskas
