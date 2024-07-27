#pragma once

#include "kaskas/prompt/dialect.hpp"
#include "kaskas/prompt/message.hpp"
#include "kaskas/prompt/rpc_result.hpp"

#include <spine/core/debugging.hpp>
#include <spine/platform/hal.hpp>

#include <initializer_list>
#include <memory>
#include <optional>
#include <string_view>

namespace kaskas::prompt {

class RPCModel {
public:
    RPCModel(const std::string& name) : _name(name) {}
    RPCModel(const std::string& name,
             const std::function<RPCResult(const OptStringView&)>& call,
             const std::string& help = "") :
        _name(name),
        _call(call),
        _help(help) {}

    const std::string_view name() const { return _name; }
    const std::string_view help() const { return _help; }
    RPCResult call(const OptStringView& value) const { return std::move(_call(value)); }

private:
    const std::string _name;
    const std::string _help;
    const std::function<RPCResult(const OptStringView&)> _call;
};

class RPCRecipeFactory;

struct RPCRecipe {
    RPCRecipe(const std::string& command, const std::initializer_list<RPCModel>& rpcs) :
        _module(command),
        _models(rpcs){};

    const std::string_view module() const { return _module; }

    const std::vector<RPCModel>& models() const { return _models; }
    std::vector<RPCModel> extract_models() { return std::move(_models); }

    std::optional<const RPCModel*> find_model_for_command(const std::string_view& key) const {
        const RPCModel* found_model = nullptr;
        for (const auto& m : _models) {
            if (m.name() == key) {
                found_model = &m;
                break;
            }
        }
        return found_model;
    }

    std::optional<std::string> help_string() const {
        if (_models.empty()) {
            return {};
        }

        size_t total_length = 0;
        for (auto& m : _models) {
            total_length += m.name().length();
        }

        std::string help;
        help.reserve(total_length + _models.size()); // account for newline

        for (auto& m : _models) {
            help += m.name();
            help += "\n";
        }
        return help;
    }

private:
    std::string _module;
    std::vector<RPCModel> _models;

    friend RPCRecipeFactory;
};

class RPCRecipeFactory {
public:
    explicit RPCRecipeFactory(const std::string& command) :
        _recipe(std::make_unique<RPCRecipe>(RPCRecipe(command, {}))) {}

    void add_model(RPCModel&& model) { _recipe->_models.emplace_back(std::move(model)); }
    std::unique_ptr<RPCRecipe> extract_recipe() { return std::move(_recipe); }

private:
    std::unique_ptr<RPCRecipe> _recipe;
};

class RPCFactory;

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

class RPCFactory {
public:
    struct Config {
        size_t directory_size = 32;
    };
    RPCFactory(const Config&& cfg) : _cfg(cfg) { _rpcs.reserve(_cfg.directory_size); }

    std::optional<RPC> from_message(const Message& msg) {
        assert(msg.operant.length() == 1);
        const auto optype = Dialect::optype_for_operant(msg.operant[0]);
        // assert(optype != Dialect::OP::NOP);

        switch (optype) {
        case Dialect::OP::NOP: {
            DBG("invalid operant {%c} found for message: {%s}", msg.operant[0], msg.as_string().c_str());
            return {};
        }
        case Dialect::OP::REQUEST: {
            const auto recipe = recipe_for_command(msg.module);
            if (!recipe) {
                DBG("no recipe found for message: {%s}", msg.as_string().c_str());
                return {};
            }
            assert(*recipe != nullptr);
            return build_rpc_for_request(**recipe, optype, msg);
        }
        case Dialect::OP::PRINT_USAGE: {
            DBG("rpc: usage request");
            return build_rpc_for_usage(msg);
        }
        default: DBG("no optype found for message: {%s}", msg.as_string().c_str()); return {};
        }
    }

    void hotload_rpc_recipe(std::unique_ptr<RPCRecipe> recipe) {
        assert(recipe);
        _rpcs.push_back(std::move(recipe));
    }

protected:
    std::optional<RPC> build_rpc_for_usage(const Message& msg) {
        static RPCModel model = {"", [&](const OptStringView&) -> RPCResult {
                                     std::string s;
                                     constexpr auto alloc_estimate = 1024;
                                     s.reserve(alloc_estimate);

                                     s += Dialect::USAGE_STRING.data();
                                     s += "\n\r";

                                     for (const auto& r : _rpcs) {
                                         for (const auto& m : r->models()) {
                                             s += "  ";
                                             s += r->module();
                                             s += ":";
                                             s += m.name();
                                             s += "\n\r";
                                         }
                                     }
                                     assert(s.size() < alloc_estimate && "raise alloc_estimate!");
                                     s.shrink_to_fit();
                                     // DBG("%s", s.c_str());
                                     return RPCResult(s);
                                 }};
        auto rpc = RPC(Dialect::OP::PRINT_USAGE, model, std::nullopt);

        return rpc;
    }

    std::optional<RPC> build_rpc_for_request(const RPCRecipe& recipe, Dialect::OP optype, const Message& msg) {
        if (!msg.cmd) {
            return {};
        }

        auto found_model = recipe.find_model_for_command(*msg.cmd);
        if (found_model == nullptr) { // no model found
            DBG("build_rpc: No model found for msg {%s}", msg.as_string().c_str());
            return {};
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
