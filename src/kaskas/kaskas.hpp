#pragma once

#include "kaskas/component.hpp"
#include "kaskas/data_providers.hpp"
#include "kaskas/events.hpp"
#include "kaskas/io/hardware_stack.hpp"
#include "kaskas/io/peripherals/relay.hpp"
#include "kaskas/io/providers/clock.hpp"
#include "kaskas/prompt/datalink.hpp"
#include "kaskas/prompt/prompt.hpp"
#include "kaskas/prompt/rpc/cookbook.hpp"
#include "kaskas/subsystems/climatecontrol.hpp"
#include "kaskas/subsystems/data_acquisition.hpp"
#include "kaskas/subsystems/fluidsystem.hpp"
#include "kaskas/subsystems/growlights.hpp"
#include "kaskas/subsystems/hardware.hpp"
#include "kaskas/subsystems/ui.hpp"

#include <spine/core/debugging.hpp>
#include <spine/core/exception.hpp>
#include <spine/eventsystem/eventsystem.hpp>
#include <spine/io/stream/stream.hpp>
#include <spine/platform/hal.hpp>
#include <spine/structure/units/si.hpp>
#include <spine/structure/vector.hpp>

#include <utility>
#include <vector>

namespace kaskas {

using kaskas::prompt::Prompt;
using spn::core::Event;
using spn::core::EventSystem;
using spn::structure::Vector;

class KasKas {
public:
    struct Config {
        EventSystem::Config es_cfg;
        uint16_t component_cap = 1;

        std::optional<Prompt::Config> prompt_cfg;
    };

public:
    explicit KasKas(std::shared_ptr<io::HardwareStack> hws, Config& cfg)
        : _cfg(cfg), _evsys({cfg.es_cfg}), _hws(std::move(hws)),
          _components(std::vector<std::unique_ptr<Component>>()) {
        if (_cfg.prompt_cfg) {
            auto uart = std::make_shared<HAL::UART>(HAL::UART::Config{.stream = &Serial, .timeout = k_time_ms(50)});
            using prompt::Datalink;
            auto dl =
                std::make_shared<Datalink>(uart, Datalink::Config{.input_buffer_size = _cfg.prompt_cfg->io_buffer_size,
                                                                  .output_buffer_size = _cfg.prompt_cfg->io_buffer_size,
                                                                  .delimiters = _cfg.prompt_cfg->line_delimiters});
            _prompt = std::make_shared<Prompt>(std::move(*_cfg.prompt_cfg));
            _prompt->hotload_datalink(std::move(dl));
        }
    }
    KasKas(const KasKas& other) = delete;
    KasKas(KasKas&& other) = delete;
    KasKas& operator=(const KasKas&) = delete;
    KasKas& operator=(const KasKas&&) = delete;
    ~KasKas() = default;

    int initialize() {
        // set the global exception handler;
        set_machine_exception_handler(std::make_unique<KasKasExceptionHandler>(*this));

        // initialize all components
        for (const auto& component : _components) {
            component->initialize();
        }

        if (_cfg.prompt_cfg) {
            {
                auto recipes = _hws->cookbook().extract_recipes();
                for (auto& r : recipes) {
                    hotload_rpc_recipe(std::move(r));
                }
            }

            _prompt->initialize();
        }

        _evsys.trigger(_evsys.event(Events::WakeUp, k_time_s(0), Event::Data()));
        LOG("Kaskas: Startup complete");
        return 0;
    }

    void hotload_component(std::unique_ptr<Component> component) {
        component->attach_event_system(&_evsys);

        auto ssf = io::VirtualStackFactory(_hws);
        component->sideload_providers(ssf);

        if (_cfg.prompt_cfg) {
            if (auto recipe = component->rpc_recipe()) {
                spn_assert(recipe != nullptr);
                hotload_rpc_recipe(std::move(recipe));
            }
        }
        _components.emplace_back(std::move(component));
    }

    void hotload_rpc_recipe(std::unique_ptr<prompt::RPCRecipe> recipe) {
        _prompt->hotload_rpc_recipe(std::move(recipe));
    }

    std::shared_ptr<Prompt> prompt() { return _prompt; }

    int loop() {
        platform_sanity_checks();
        _evsys.loop();
        return 0;
    }

private:
    void platform_sanity_checks() {
        if (HAL::free_memory() < 1024) { // it is good to have no leaks, it is better to be safe
            spn::throw_exception(spn::runtime_exception("Less than a kilobyte of free memory. Halting."));
        }
    }

private:
    class KasKasExceptionHandler final : public spn::core::ExceptionHandler {
    public:
        KasKasExceptionHandler(KasKas& kaskas) : _kk(kaskas) {}

        void handle_exception(const spn::core::Exception& exception) override {
            DBG("KasKasExceptionHandler: Handling exception: %s", exception.error_type());
            for (auto& sf : _kk._components) {
                sf->safe_shutdown(Component::State::CRITICAL);
            }
            HAL::halt("KasKas: halting after exception");
        }

    private:
        const KasKas& _kk;
    };

private:
    Config _cfg;
    EventSystem _evsys;
    std::shared_ptr<io::HardwareStack> _hws;
    std::vector<std::unique_ptr<Component>> _components;
    std::shared_ptr<Prompt> _prompt;
};
} // namespace kaskas
