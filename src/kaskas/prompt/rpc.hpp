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
    explicit RPCModel(const std::string_view& name) : _name(name) {}
    RPCModel(const std::string_view& name, const std::function<RPCResult(const OptString&)>& call)
        : _name(name), _call(call) {
        const auto s = std::string(name);
        DBGF("initializing RPCModel with name %s", s.c_str());
    }

    std::string_view name() const { return _name; }
    std::function<RPCResult(const OptString&)> call() const { return _call; }

private:
    const std::string_view _name;
    const std::function<RPCResult(const OptString&)> _call;
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

struct RPCRecipe {
    // using RPCVariant = std::variant<RWVariable, ROVariable, FunctionCall>;

    RPCRecipe(const char* command, const std::initializer_list<RPCModel>& rpcs)
        : _command(command), _rpcs(std::move(rpcs)){};

    std::optional<const RPCModel> rpc_from_arguments(const std::string_view& arguments) const {
        // for (const auto& rpc_v : _rpcs) {
        // const auto cmd = std::visit([](auto&& v) { return v.name(); }, rpc_v).data();
        // }
    }

    const char* command() const {
        assert(_command);
        return _command;
    }
    const std::vector<RPCModel> rpcs() const { return _rpcs; }

    // protected:
private:
    const char* _command;
    const std::vector<RPCModel> _rpcs;
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
    const RPCModel model;

    const OptStringView key;
    const OptStringView value;

    RPCResult invoke() const {
        DBGF("invoking RPC!");

        // const auto& m = model_for_op(model, op);
        // assert(model);
        // const auto model = *m;
        const auto args = value ? std::make_optional(std::string(*value)) : std::nullopt;
        const auto res = model.call()(args);

        assert(res.return_value);
        assert(res.status);
        DBGF("has res with status: %i, rstring: %s", *res.status, res.return_value->c_str());
        return res;
    }

public:
protected:
    RPC(Dialect::OP op, const RPCModel& model) : op(op), model(model) {}

    friend RPCFactory;
};

class RPCFactory {
public:
    struct Config {
        size_t directory_size = 32;
    };
    RPCFactory(const Config&& cfg) : _cfg(cfg), _rpcs() { _rpcs.reserve(_cfg.directory_size); }

    std::optional<RPC> from_message(const Message& msg) {
        assert(msg.operant().length() == 1);
        const auto recipe = recipe_for_command(msg.cmd());
        if (!recipe)
            return {};
        assert(*recipe != nullptr);

        const auto s = std::string(msg.operant());
        DBGF("operant :%s", s.c_str());
        DBGF("operant :%c", s.c_str()[0]);
        const auto optype = Dialect::optype_for_operant(msg.operant()[0]);
        assert(optype != Dialect::OP::NOP);

        switch (optype) {
        case Dialect::OP::ACCESS: return build_ro_variable(**recipe, optype, msg.value());
        case Dialect::OP::ASSIGNMENT: return build_rw_variable(**recipe, optype, msg.value());
        case Dialect::OP::FUNCTION_CALL: return build_function_call(**recipe, optype, msg.value());
        default: return {};
        }
    }

    void hotload_rpc_recipe(std::unique_ptr<RPCRecipe> recipe) { _rpcs.push_back(std::move(recipe)); }

protected:
    std::optional<RPC> build_function_call(const RPCRecipe& recipe, Dialect::OP optype,
                                           const std::string_view& arguments) {}
    std::optional<RPC> build_ro_variable(const RPCRecipe& recipe, Dialect::OP optype,
                                         const std::string_view& arguments) {
        DBGF("building ro variable");
        const auto key = arguments;

        assert(recipe.rpcs().size() > 0);
        const auto model = std::find_if(std::begin(recipe.rpcs()), std::end(recipe.rpcs()), [key](const RPCModel& m) {
            // const std::string_view name = std::visit(
            //     [](const RPCModel& v) -> const std::string_view {
            //         const auto s = std::string(v.name());
            //         DBGF("visiting: %s", s.c_str());
            //         return v.name();
            //     },
            //     m);

            // const auto k = std::string(key);
            // const auto n = std::string(m.name());
            // DBGF("key: %s, name: %s", k.c_str(), n.c_str());

            return key == m.name();
        });

        if (model == std::end(recipe.rpcs())) {
            // no model found
            DBG("no model found");
            return {};
        }

        return RPC(optype, *model);
    }
    std::optional<RPC> build_rw_variable(const RPCRecipe& recipe, Dialect::OP optype,
                                         const std::string_view& arguments) {
        const auto seperator_idx = arguments.find(':');
        if (seperator_idx == std::string::npos) {
            // no separator found
            return {};
        }
        // found separator
        const auto key = arguments.substr(0, seperator_idx);
        const auto value = arguments.substr(seperator_idx, arguments.length());

        const auto model = std::find_if(std::begin(recipe.rpcs()), std::end(recipe.rpcs()), [key](const RPCModel& m) {
            // const auto name = std::visit([](auto&& v) { return v.name(); }, m);
            return key == m.name();
        });
        if (model == std::end(recipe.rpcs())) {
            // no model found
            return {};
        }
        return RPC(optype, *model);
    }

    std::optional<const RPCRecipe*> recipe_for_command(const std::string_view& cmd) {
        assert(_rpcs.size() == 1);

        for (const auto& recipe : _rpcs) {
            assert(recipe->command());
            if (std::string_view(recipe->command()) == cmd) {
                const auto cmd_str = std::string(cmd);
                DBGF("RPCHandler: found recipe: %s for %s", recipe->command(), cmd_str.c_str())
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