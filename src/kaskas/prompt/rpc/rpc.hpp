#pragma once

#include "kaskas/prompt/dialect.hpp"
#include "kaskas/prompt/message/message.hpp"
#include "kaskas/prompt/rpc/model.hpp"
#include "kaskas/prompt/rpc/recipe.hpp"
#include "kaskas/prompt/rpc/result.hpp"

#include <spine/core/debugging.hpp>
#include <spine/core/logging.hpp>
#include <spine/platform/hal.hpp>
#include <spine/structure/result.hpp>

#include <initializer_list>
#include <memory>
#include <optional>
#include <string_view>
#include <type_traits>

namespace kaskas::prompt {

class RPCFactory; // forward declaration

/// An invokable remote procedure call.
class RPC {
public:
    const Dialect::OP op;
    const RPCModel& model;

    const OptString value;

    RPCResult invoke() const { return model.call(value); }

public:
protected:
    RPC(Dialect::OP op, const RPCModel& model, OptString value) : op(op), model(model), value(value) {}

    friend RPCFactory;
};

/// Builds RPC's from stored recipes.
class RPCFactory {
public:
    struct Config {
        size_t directory_size = 32;
    };

    enum class Error : uint8_t { INVALID_OPERANT, UNKNOWN_RECIPE, UNKNOWN_MODEL, MALFORMED_MESSAGE };

    RPCFactory(const Config&& cfg) : _cfg(cfg) { _rpcs.reserve(_cfg.directory_size); }
    RPCFactory(const Config& cfg) : RPCFactory(Config(cfg)) {}

    spn::structure::Result<RPC, Error> from_message(const Message& msg) {
        assert(msg.operant.length() == 1);
        const auto optype = Dialect::optype_for_operant(msg.operant[0]);

        switch (optype) {
        case Dialect::OP::NOP: {
            DBG("RPCFactory: invalid operant {%c} found for message: {%s}", msg.operant[0], msg.as_string().c_str());
            return spn::structure::Result<RPC, Error>::failed(Error::INVALID_OPERANT);
        }
        case Dialect::OP::REQUEST: {
            const auto recipe = recipe_for_command(msg.module);
            if (!recipe) {
                DBG("RPCFactory: no recipe found for message: {%s}", msg.as_string().c_str());
                return spn::structure::Result<RPC, Error>::failed(Error::UNKNOWN_RECIPE);
            }
            assert(*recipe != nullptr);
            return build_rpc_for_request(**recipe, optype, msg);
        }
        case Dialect::OP::PRINT_USAGE: {
            return build_rpc_for_usage(msg);
        }
        default:
            DBG("RPCFactory: no optype found for message: {%s}", msg.as_string().c_str());
            return spn::structure::Result<RPC, Error>::failed(Error::INVALID_OPERANT);
        }
    }

    void hotload_rpc_recipe(std::unique_ptr<RPCRecipe> recipe) {
        assert(recipe);
        _rpcs.push_back(std::move(recipe));
    }

protected:
    spn::structure::Result<RPC, Error> build_rpc_for_usage(const Message& msg) {
        static RPCModel model = {"", [&](const OptStringView&) -> RPCResult {
                                     auto write_to_string = [&](std::string* s, bool length_only) -> size_t {
                                         size_t len = 0;
                                         auto put = [&](std::string_view str) {
                                             if (!length_only && s != nullptr) {
                                                 *s += str;
                                             }
                                             len += str.size();
                                         };

                                         put("[");
                                         put(Dialect::API_VERSION);
                                         put("]");
                                         put("\n\r");

                                         for (const auto& r : _rpcs) {
                                             for (const auto& m : r->models()) {
                                                 put("  ");
                                                 put(r->module());
                                                 put(":");
                                                 put(m.name());
                                                 put("\n\r");
                                             }
                                         }
                                         return len;
                                     };

                                     // First pass to calculate the total length
                                     size_t reserved_length = write_to_string(nullptr, true);

                                     // Reserve capacity to avoid multiple allocations
                                     std::string s{};
                                     s.reserve(reserved_length);
                                     reserved_length = s.capacity(); // for sanity checks below

                                     // Second pass to build the string
                                     write_to_string(&s, false);

                                     assert(s.size() <= reserved_length); // no reallocation
                                     assert(s.capacity() == reserved_length); // no reallocation
                                     return RPCResult(s);
                                 }};
        return RPC(Dialect::OP::PRINT_USAGE, model, std::nullopt);
    }

    spn::structure::Result<RPC, Error> build_rpc_for_request(const RPCRecipe& recipe, Dialect::OP optype,
                                                             const Message& msg) {
        if (msg.cmd_or_status.size() == 0) {
            return spn::structure::Result<RPC, Error>::failed(Error::MALFORMED_MESSAGE);
        }

        auto found_model = recipe.find_model_for_command(msg.cmd_or_status);
        if (found_model == nullptr) { // no model found
            WARN("RPCFactory: No model found for msg {%s}", msg.as_string().c_str());
            return spn::structure::Result<RPC, Error>::failed(Error::UNKNOWN_MODEL);
        }

        const auto opt_value =
            msg.arguments && !msg.arguments->empty() ? std::make_optional(std::string(*msg.arguments)) : std::nullopt;
        return RPC(optype, *found_model.value(), opt_value);
    }

    std::optional<const RPCRecipe*> recipe_for_command(const std::string_view& cmd) {
        for (const auto& recipe : _rpcs) {
            assert(recipe->module() != std::string_view{});
            if (std::string_view(recipe->module()) == cmd) {
                const auto cmd_str = std::string(cmd);
                const auto recipe_cmd_str = std::string(cmd);
                return recipe.get();
            }
        }
        return {};
    }

private:
    const Config _cfg;

    std::vector<std::unique_ptr<RPCRecipe>> _rpcs;
};

} // namespace kaskas::prompt
