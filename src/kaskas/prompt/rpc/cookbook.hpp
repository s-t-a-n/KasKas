#pragma once

#include "kaskas/prompt/rpc/rpc.hpp"

#include <list>
#include <set>

namespace kaskas::prompt {

/// A cookbook of RPCRecipes. To be used when directly hotloading RPCRecipes into the prompt is not feasible. For
/// example when a module must be initialized before the prompt is ready.
class RPCCookbook {
public:
    // todo: make a static version that doesnt employ a list as it causes fragmentation of heap.
    using Recipes = std::list<std::unique_ptr<RPCRecipe>>;

    Recipes&& extract_recipes() {
        consolidate_recipes();
        return std::move(_recipes);
    }
    void add_recipe(std::unique_ptr<RPCRecipe> recipe) {
        if (recipe) _recipes.emplace_back(std::move(recipe));
    }

protected:
    /// Consolidate recipes by reordering them
    /// make sure models in recipes with the same `cmd` name are stored in the same recipe.
    void consolidate_recipes() {
        auto new_cookbook = Recipes();

        // extract all names
        std::set<std::string_view> names;
        for (const auto& rpc_recipe : _recipes) {
            names.insert(rpc_recipe->module());
        }

        for (const auto& name : names) {
            const auto name_str = std::string(name);
            DBG("RPCCookbook: consolidating %s", name_str.c_str());
            RPCRecipeFactory rf(name_str);

            for (auto& r : _recipes) {
                if (r->module() == name) {
                    for (auto& m : r->extract_models()) {
                        DBG("-> %s", std::string(m.name()).c_str());
                        rf.add_model(std::move(m));
                    }
                }
            }
            new_cookbook.emplace_back(std::move(rf.extract_recipe()));
        }
        _recipes.swap(new_cookbook); // overwrite old cookbook
    }

private:
    Recipes _recipes;
};
} // namespace kaskas::prompt
