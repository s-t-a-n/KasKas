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
        if (recipe)
            _recipes.emplace_back(std::move(recipe));
    }

protected:
    // make sure models in recipes with the same `cmd` name are stored in the same recipe.
    void consolidate_recipes() {
        auto new_cookbook = CookBook();

        // extract all names
        std::set<std::string_view> names;
        for (const auto& rpc_recipe : _recipes) {
            names.insert(rpc_recipe->command());
        }

        for (const auto& name : names) {
            const auto name_str = std::string(name);
            DBGF("consolidating %s", name_str.c_str());
            RPCRecipeFactory rf(name_str);

            for (auto& r : _recipes) {
                if (r->command() == name) {
                    // extract this recipe
                    for (auto& m : r->extract_models()) {
                        DBGF("-> %s", std::string(m.name()).c_str());
                        rf.add_model(std::move(m));
                    }
                    // get rid off old recipe
                    // r.reset();
                } else {
                    DBGF("wtf: cmd %s, name :%s", std::string(r->command()).c_str(), std::string(name).c_str())
                }
            }
            new_cookbook.emplace_back(std::move(rf.extract_recipe()));
        }

        // // get rid off old recipes
        // for (auto& r : _recipes) {
        //     r.reset();
        // }

        // overwrite old cookbook
        _recipes.clear();
        _recipes = std::move(new_cookbook);
        // while (!_recipes.empty()) {
        //     std::string_view next_recipe_name;
        //
        //     for (auto& recipe : _recipes) {
        //     }
        // }
    }

private:
    CookBook _recipes;
};
} // namespace kaskas::prompt