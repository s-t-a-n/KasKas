#pragma once

#include "io/peripherals/relay.hpp"
#include "kaskas/component.hpp"
#include "kaskas/data_providers.hpp"
#include "kaskas/events.hpp"
#include "kaskas/io/providers/clock.hpp"
#include "kaskas/io/stack.hpp"
#include "kaskas/prompt/prompt.hpp"
#include "kaskas/subsystems/climatecontrol.hpp"
#include "kaskas/subsystems/fluidsystem.hpp"
#include "kaskas/subsystems/growlights.hpp"
#include "kaskas/subsystems/hardware.hpp"
#include "kaskas/subsystems/metrics.hpp"
#include "kaskas/subsystems/ui.hpp"

#include <spine/core/exception.hpp>
#include <spine/core/time.hpp>
#include <spine/eventsystem/eventsystem.hpp>
#include <spine/platform/hal.hpp>
#include <spine/structure/pointer.hpp>
#include <spine/structure/vector.hpp>

#include <AH/STL/vector>
#include <utility>
namespace kaskas {

using kaskas::prompt::Prompt;
using spn::core::Event;
using spn::core::EventSystem;
using spn::structure::Vector;

class KasKas {
public:
    struct Config {
        EventSystem::Config esc_cfg;
        uint16_t component_cap = 1;

        std::optional<Prompt::Config> prompt_cfg;
    };

public:
    explicit KasKas(std::shared_ptr<io::HardwareStack> hws, Config& cfg)
        : _cfg(cfg), _evsys({cfg.esc_cfg}), _hws(std::move(hws)),
          _components(std::vector<std::unique_ptr<Component>>()) {}
    KasKas(const KasKas& other) = delete;
    KasKas(KasKas&& other) = delete;
    KasKas& operator=(const KasKas&) = delete;
    KasKas& operator=(const KasKas&&) = delete;

    ~KasKas() = default;

    int initialize() {
        // set the global exception handler;
        set_machine_exception_handler(new KasKasExceptionHandler{*this});

        // initialize all components
        for (const auto& component : _components) {
            component->initialize();
        }

        if (_cfg.prompt_cfg) {
            using prompt::SerialDatalink;
            auto dl = std::make_unique<SerialDatalink>(
                SerialDatalink::Config{.message_length = _cfg.prompt_cfg->message_length,
                                       .pool_size = _cfg.prompt_cfg->pool_size},
                HAL::UART(HAL::UART::Config{&Serial}));
            _prompt = std::make_unique<Prompt>(std::move(*_cfg.prompt_cfg));
            _prompt->get()->hotload_datalink(std::move(dl));
            _prompt->get()->initialize();
        }

        _evsys.trigger(_evsys.event(Events::WakeUp, time_s(0), Event::Data()));
        return 0;
    }

    void hotload_component(std::unique_ptr<Component> component) {
        component->attach_event_system(&_evsys);

        if (_cfg.prompt_cfg) {
            if (auto model = component->rpc_recipe())
                _prompt->get()->hotload_rpc_recipe(std::move(model));
        }
        _components.emplace_back(std::move(component));
    }

    EventSystem& evsys() { return _evsys; };

    int loop() {
        _hws->update_all();
        _evsys.loop();
        return 0;
    }

private:
    class KasKasExceptionHandler final : public spn::core::ExceptionHandler {
    public:
        KasKasExceptionHandler(KasKas& kaskas) : _kk(kaskas) {}

        void handle_exception(const spn::core::Exception& exception) override {
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
    std::shared_ptr<io::HardwareStack> _hws;
    std::vector<std::unique_ptr<Component>> _components;
    std::optional<std::unique_ptr<Prompt>> _prompt = nullptr;
};
} // namespace kaskas