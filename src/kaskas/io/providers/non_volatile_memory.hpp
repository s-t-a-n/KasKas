#pragma once

#include "kaskas/io/provider.hpp"

namespace kaskas::io {

class NonVolatileMemory : public Provider {
public:
    std::unique_ptr<prompt::RPCRecipe> rpc_recipe(const std::string_view& recipe_name, const std::string_view& root) {
        return {};
    }

private:
};

} // namespace kaskas::io