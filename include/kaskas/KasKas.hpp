#pragma once

#include "kaskas/component.hpp"
#include "kaskas/events.hpp"
#include "kaskas/io/clock.hpp"
#include "kaskas/io/relay.hpp"
#include "kaskas/subsystems/fluidsystem.hpp"
#include "kaskas/subsystems/growlights.hpp"
#include "kaskas/subsystems/ui.hpp"
#include "kaskas/subsystems/ventilation.hpp"

#include <spine/core/exception.hpp>
#include <spine/core/time.hpp>
#include <spine/eventsystem/eventsystem.hpp>
#include <spine/platform/hal.hpp>
#include <spine/structure/pointer.hpp>
#include <spine/structure/vector.hpp>

#include <AH/STL/vector>
namespace kaskas {

using spn::core::Event;
using spn::core::EventSystem;
using spn::structure::Vector;

class KasKas {
public:
    struct Config {
        EventSystem::Config esc_cfg;
        uint16_t component_cap = 1;
    };

public:
    explicit KasKas(Config& cfg)
        : _cfg(cfg), //
          _evsys({cfg.esc_cfg}), //
          _components(std::vector<std::unique_ptr<Component>>()) {}
    KasKas(const KasKas& other) = delete;
    KasKas(KasKas&& other) = delete;
    KasKas& operator=(const KasKas&) = delete;
    KasKas& operator=(const KasKas&&) = delete;

    ~KasKas() = default;

    int initialize() {
        // set the global exception handler;
        spn::core::set_machine_exception_handler(new KasKasExceptionHandler{*this});

        // initialize all components
        for (const auto& component : _components) {
            component->initialize();
        }
        _evsys.trigger(_evsys.event(Events::WakeUp, time_s(0), Event::Data()));
        return 0;
    }

    void hotload_component(std::unique_ptr<Component> component) {
        //
        component->attach_event_system(&_evsys);
        _components.emplace_back(std::move(component));
    }

    EventSystem& evsys() { return _evsys; };

    int loop() {
        _evsys.loop();
        return 0;
    }

private:
    class KasKasExceptionHandler final : public spn::core::ExceptionHandler {
    public:
        KasKasExceptionHandler(KasKas& kaskas) : _kk(kaskas) {}

        void handle_exception(const Exception& exception) override {
            //
            DBGF("KasKasExceptionHandler: Handling exception: %s", exception.error_type());
            for (auto& sf : _kk._components) {
                sf->safe_shutdown(Component::State::CRITICAL);
            }
        }

    private:
        const KasKas& _kk;
    };

private:
    Config _cfg;
    EventSystem _evsys;
    std::vector<std::unique_ptr<Component>> _components;
};
} // namespace kaskas