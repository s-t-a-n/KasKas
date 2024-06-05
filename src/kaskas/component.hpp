#pragma once

#include <spine/eventsystem/eventsystem.hpp>

namespace kaskas {

using spn::core::EventHandler;
using spn::core::EventSystem;

class Component : public EventHandler {
public:
    enum class State { SAFE, CRITICAL };

public:
    explicit Component() : Component(nullptr) {}
    explicit Component(EventSystem* event_system) : EventHandler(event_system) {}

    // virtual Component& operator=(Component&& other) {}
    virtual void initialize() { assert(!"Virtual base function called"); }
    virtual void safe_shutdown(State state = State::SAFE) { assert(!"Virtual base function called"); }

private:
};

} // namespace kaskas