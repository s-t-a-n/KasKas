#pragma once

#include "kaskas/io/peripheral.hpp"
#include "kaskas/io/peripherals/fan.hpp"
#include "kaskas/io/provider.hpp"
#include "kaskas/io/providers/analogue.hpp"
#include "kaskas/io/providers/clock.hpp"
#include "kaskas/io/providers/digital.hpp"
#include "kaskas/prompt/cookbook.hpp"

#include <spine/core/standard.hpp>
#include <spine/structure/array.hpp>

#include <memory>
#include <utility>

namespace kaskas::io {
using ::spn::structure::Array;

// todo: swap alias with base, make ContinuesValue the base, etc
using ContinuousValue = AnalogueSensor;
using LogicalValue = DigitalSensor;

class VirtualStackFactory;

///
class VirtualStack {
public:
    using Idx = uint16_t;

    struct Config {
        std::string_view alias;
        Idx max_providers = 16;
    };

    VirtualStack(const Config&& cfg) : _providers(cfg.max_providers), _cfg(std::move(cfg)) {}

public:
    const std::string_view& alias() const { return _cfg.alias; }
    prompt::RPCCookbook& cookbook() { return _rpc_cookbook; }

public:
    const ContinuousValue& continuous_value(Idx provider_idx) {
        assert(_providers[provider_idx]);
        return *reinterpret_cast<ContinuousValue*>(_providers[provider_idx].get());
    }

protected:
    Array<std::shared_ptr<Provider>> _providers;
    prompt::RPCCookbook _rpc_cookbook;

private:
    const Config _cfg;

    friend VirtualStackFactory;
};

class VirtualStackFactory {
public:
    VirtualStackFactory(const VirtualStack::Config&& cfg) : _stack(std::make_shared<VirtualStack>(std::move(cfg))) {}
    VirtualStackFactory(std::shared_ptr<VirtualStack> stack) : _stack(std::move(stack)) {}

    void hotload_provider(DataProviders provider_id,
                          std::shared_ptr<Provider> provider,
                          const std::optional<std::string_view>& module_name = {}) {
        assert(ENUM_IDX(provider_id) < _stack->_cfg.max_providers);
        assert(_stack->_providers[ENUM_IDX(provider_id)] == nullptr);
        stack()->cookbook().add_recipe(std::move(
            provider->rpc_recipe(module_name.value_or(stack()->alias()), magic_enum::enum_name(provider_id))));
        _stack->_providers[ENUM_IDX(provider_id)] = std::move(provider);
    }

    std::shared_ptr<VirtualStack> stack() { return _stack; }

protected:
    std::shared_ptr<VirtualStack> _stack;
};

} // namespace kaskas::io