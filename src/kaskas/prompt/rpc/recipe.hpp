#pragma once

#include "kaskas/prompt/dialect.hpp"
#include "kaskas/prompt/message/message.hpp"
#include "kaskas/prompt/rpc/model.hpp"
#include "kaskas/prompt/rpc/result.hpp"

#include <spine/core/debugging.hpp>
#include <spine/platform/hal.hpp>
#include <spine/structure/result.hpp>

#include <initializer_list>
#include <memory>
#include <optional>
#include <string_view>
#include <type_traits>

namespace kaskas::prompt {

class RPCRecipeFactory;

/// A recipe of RPCModels
class RPCRecipe {
public:
    RPCRecipe(const std::string_view& module, const std::initializer_list<RPCModel>& rpcs)
        : _module(module), _models(rpcs){};

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

private:
    const std::string _module;
    std::vector<RPCModel> _models;

    friend RPCRecipeFactory;
};

/// A factory for building RPCRecipes from RPCModels
class RPCRecipeFactory {
public:
    explicit RPCRecipeFactory(const std::string& command)
        : _recipe(std::make_unique<RPCRecipe>(RPCRecipe(command, {}))) {}

    void add_model(RPCModel&& model) { _recipe->_models.emplace_back(std::move(model)); }
    std::unique_ptr<RPCRecipe> extract_recipe() { return std::move(_recipe); }

private:
    std::unique_ptr<RPCRecipe> _recipe;
};

} // namespace kaskas::prompt
