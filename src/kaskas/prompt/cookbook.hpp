#pragma once

#include <spine/structure/list.hpp>
// #include <spine/structure/vector.hpp>

#include "kaskas/prompt/rpc.hpp"

#include <set>

namespace kaskas::prompt {

class RPCCookbook {
public:
    using CookBook = spn::structure::List<std::unique_ptr<RPCRecipe>>;

    CookBook&& extract_recipes() {
        consolidate_recipes();
        return std::move(_recipes);
    }
    void add_recipe(std::unique_ptr<RPCRecipe> recipe) {
        if (recipe) _recipes.emplace_back(std::move(recipe));
    }

protected:
    // make sure models in recipes with the same `cmd` name are stored in the same recipe.
    void consolidate_recipes() {
        auto new_cookbook = CookBook();

        // extract all names
        std::set<std::string_view> names;
        for (const auto& rpc_recipe : _recipes) {
            names.insert(rpc_recipe->module());
        }

        for (const auto& name : names) {
            const auto name_str = std::string(name);
            DBG("consolidating %s", name_str.c_str());
            RPCRecipeFactory rf(name_str);

            for (auto& r : _recipes) {
                if (r->module() == name) { // extract this recipe
                    for (auto& m : r->extract_models()) {
                        DBG("-> %s", std::string(m.name()).c_str());
                        rf.add_model(std::move(m));
                    }
                }
            }
            new_cookbook.emplace_back(std::move(rf.extract_recipe()));
        }

        // overwrite old cookbook
        _recipes.clear();
        _recipes = std::move(new_cookbook);
    }

private:
    CookBook _recipes;
};
} // namespace kaskas::prompt
