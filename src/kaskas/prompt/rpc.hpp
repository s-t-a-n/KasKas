#pragma once

#include "kaskas/prompt/dialect.hpp"
#include "kaskas/prompt/message.hpp"
#include "kaskas/prompt/rpc_result.hpp"

#include <spine/core/assert.hpp>
#include <spine/core/debugging.hpp>
#include <spine/platform/hal.hpp>

#include <AH/STL/memory>
#include <initializer_list>
#include <optional>
#include <string_view>

namespace kaskas::prompt {

// using OptString = std::optional<std::string>;
// using OptStringView = std::optional<std::string_view>;
// using OptInt = std::optional<int>;

class RPCModel {
public:
    RPCModel(const std::string& name) : _name(name) {}
    RPCModel(const std::string& name, const std::function<RPCResult(const OptStringView&)>& call)
        : _name(name), _call(call) {
        const auto s = std::string(name);
        // DBGF("initializing RPCModel with name %s", s.c_str());
    }

    std::string_view name() const { return _name; }
    RPCResult call(const OptStringView& value) const { return std::move(_call(value)); }

private:
    const std::string _name;
    const std::function<RPCResult(const OptStringView&)> _call;
};

// stubs
using RWVariable = RPCModel;
using ROVariable = RPCModel;
using FunctionCall = RPCModel;

// class RWVariable : public RPCModel {
// public:
//     using RPCModel::RPCModel;
// };
//
// class ROVariable : public RPCModel {
// public:
//     using RPCModel::RPCModel;
// };
//
// class FunctionCall : public RPCModel {
// public:
//     using RPCModel::RPCModel;
// };

class RPCRecipeFactory;

struct RPCRecipe {
    // using RPCVariant = std::variant<RWVariable, ROVariable, FunctionCall>;

    // RPCRecipe(const std::string& command, const std::initializer_list<RPCModel>& rpcs)
    // : RPCRecipe(command.c_str(), rpcs) {}

    RPCRecipe(const std::string& command, const std::initializer_list<RPCModel>& rpcs)
        : _command(command), _models(std::move(rpcs)){};

    // std::optional<const RPCModel> rpc_from_arguments(const std::string_view& arguments) const {
    //     // for (const auto& rpc_v : _rpcs) {
    //     // const auto cmd = std::visit([](auto&& v) { return v.name(); }, rpc_v).data();
    //     // }
    // }

    const std::string_view command() const { return _command; }

    const std::vector<RPCModel>& models() const { return _models; }
    std::vector<RPCModel> extract_models() { return std::move(_models); }

    std::optional<const RPCModel*> find_model_for_key(const std::string_view& key) const {
        // assert(key == std::string_view("CLIMATE_HUMIDITY"));
        // assert(key.length() == 16);

        const RPCModel* found_model = nullptr;
        for (const auto& m : _models) {
            // DBGF("looking through model {%s} for key {%s}", std::string(m.name()).c_str(), std::string(key).c_str());
            if (m.name() == key) {
                found_model = &m;
                // DBGF("found model!");
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

    // protected:
private:
    std::string _command;
    std::vector<RPCModel> _models;

    friend RPCRecipeFactory;
};

class RPCRecipeFactory {
public:
    explicit RPCRecipeFactory(const std::string& command)
        : _recipe(std::make_unique<RPCRecipe>(RPCRecipe(command, {}))) {}

    void add_model(RPCModel&& model) { _recipe->_models.emplace_back(std::move(model)); }
    std::unique_ptr<RPCRecipe> extract_recipe() { return std::move(_recipe); }

private:
    std::unique_ptr<RPCRecipe> _recipe;
};

class RPCFactory;

// static std::optional<const RPCModel*> model_for_op(const RPCModel& v, Dialect::OP op) {
//     switch (op) {
//     case Dialect::OP::ASSIGNMENT: return &std::get<RWVariable>(v);
//     case Dialect::OP::ACCESS: return &std::get<ROVariable>(v);
//     case Dialect::OP::FUNCTION_CALL: return &std::get<FunctionCall>(v);
//     default: return {};
//     }
// }

class RPC {
public:
    const Dialect::OP op;
    const RPCModel& model;

    const OptString value;

    RPCResult invoke() const {
        const auto key = std::string(model.name());
        const auto value_str = value ? std::string(*value) : std::string();
        // DBGF("Invoking RPC{%s} with value:{%s}!", key.c_str(), value_str.c_str());

        // const auto& m = model_for_op(model, op);
        // assert(model);
        // const auto model = *m;

        // DBGF("hello");
        // while (true) {
        // };
        // HAL::println("before invoke");
        // HAL::delay(time_ms(200));
        // if (value) {
        //     HAL::println("value");
        //     HAL::delay(time_ms(200));
        // } else {
        //     HAL::println("no value");
        //     HAL::delay(time_ms(200));
        // }

        const auto res = model.call(value);
        // HAL::println("after invoke");
        // HAL::delay(time_ms(200));
        // DBGF("hello");
        // while (true) {
        // };

        assert(res.return_value);
        // assert(res.status);
        // DBGF("has res with status: %i, rstring: %s", *res.status, res.return_value->c_str());
        return res;
    }

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
        // DBGF("from message : {%s}", msg.as_string().c_str());

        assert(msg.operant().length() == 1);
        const auto recipe = recipe_for_command(msg.cmd());
        if (!recipe) {
            DBGF("no recipe found for message: {%s}", msg.as_string().c_str());
            // Serial.println("no recipe found");
            return {};
        }
        assert(*recipe != nullptr);

        // const auto s = std::string(msg.operant());
        // DBGF("operant :%s", s.c_str());
        // DBGF("operant :%c", s.c_str()[0]);
        const auto optype = Dialect::optype_for_operant(msg.operant()[0]);
        assert(optype != Dialect::OP::NOP);

        if (!msg.key()) {
            // the monkey wants to know what to write
        }

        switch (optype) {
        case Dialect::OP::ACCESS: return build_rpc(**recipe, optype, msg);
        case Dialect::OP::ASSIGNMENT: return build_rpc(**recipe, optype, msg);
        case Dialect::OP::FUNCTION_CALL: return build_rpc(**recipe, optype, msg);
        default:
            DBGF("no optype found for message: {%s}", msg.as_string().c_str());
            Serial.println("no recipe found");
            return {};
        }
    }

    void hotload_rpc_recipe(std::unique_ptr<RPCRecipe> recipe) {
        assert(recipe);
        _rpcs.push_back(std::move(recipe));
    }

protected:
    std::optional<RPC> build_rpc(const RPCRecipe& recipe, Dialect::OP optype, const Message& msg) {
        // DBGF("build_rpc: start : {%s}", msg.as_string().c_str());

        if (!msg.key()) {
            return {};
        }

        auto found_model = recipe.find_model_for_key(*msg.key());
        if (found_model == nullptr) {
            // no model found
            DBGF("build_rpc: No model found for msg {%s}", msg.as_string().c_str());
            // Serial.println("no model found");
            return {};
        }

        const auto opt_value = msg.value() ? std::make_optional(std::string(*msg.value())) : std::nullopt;
        return RPC(optype, *found_model.value(), opt_value);
    }

    std::optional<const RPCRecipe*> recipe_for_command(const std::string_view& cmd) {
        // assert(_rpcs.size() == 1);

        for (const auto& recipe : _rpcs) {
            assert(recipe->command() != std::string_view{});
            if (std::string_view(recipe->command()) == cmd) {
                const auto cmd_str = std::string(cmd);
                const auto recipe_cmd_str = std::string(cmd);
                // DBGF("RPCHandler: found recipe: %s for %s", recipe_cmd_str.c_str(), cmd_str.c_str());
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