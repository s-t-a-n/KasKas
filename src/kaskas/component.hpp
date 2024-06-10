#pragma once

#include "kaskas/io/stack.hpp"
#include "kaskas/prompt/prompt.hpp"

#include <spine/eventsystem/eventsystem.hpp>

#include <utility>

namespace kaskas {

using prompt::Prompt;
using spn::core::EventHandler;
using spn::core::EventSystem;

class Component : public EventHandler {
public:
    enum class State { SAFE, CRITICAL };

public:
    // explicit Component() : Component(nullptr, nullptr) {}
    explicit Component(io::HardwareStack& hws) : Component(nullptr, hws) {}
    explicit Component(EventSystem* event_system, io::HardwareStack& hws) : EventHandler(event_system), _hws(hws) {}

    // virtual Component& operator=(Component&& other) {}
    virtual void initialize() { assert(!"Virtual base function called"); }
    virtual void safe_shutdown(State state = State::SAFE) { assert(!"Virtual base function called"); }

    virtual std::unique_ptr<prompt::RPCRecipe> rpc_recipe() { assert(!"Virtual base function called"); }

protected:
    io::HardwareStack& _hws;

private:
};

} // namespace kaskas