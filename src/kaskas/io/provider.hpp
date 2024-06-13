#pragma once

#include "kaskas/data_providers.hpp"
#include "kaskas/prompt/rpc.hpp"

namespace kaskas::io {

/// Encapsulates a single provider of data
class Provider {
public:
    virtual std::unique_ptr<prompt::RPCRecipe> rpc_recipe(const std::string_view& recipe_name,
                                                          const std::string_view& root) {
        assert(!"Virtual base function called");
    }

protected:
private:
};

} // namespace kaskas::io