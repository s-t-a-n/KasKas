#pragma once

#include "kaskas/prompt/datalink.hpp"
#include "kaskas/prompt/dialect.hpp"
#include "kaskas/prompt/message/incoming_message_factory.hpp"
#include "kaskas/prompt/message/message.hpp"
#include "kaskas/prompt/message/outgoing_message_factory.hpp"
#include "kaskas/prompt/rpc/rpc.hpp"

#include <spine/core/debugging.hpp>
#include <spine/platform/hal.hpp>

#include <cstring>
#include <memory>
#include <optional>
#include <string_view>

namespace kaskas::prompt {

namespace detail {

} // namespace detail

class Prompt {
public:
    struct Config {
        size_t io_buffer_size; // size of input and output buffer
        const std::string_view line_delimiters = "\r\n"; // delimiters to split input with
        size_t max_recipes_count = 32; // exact or maximum amount of recipes loadable
    };

    Prompt(const Config&& cfg)
        : _cfg(cfg), _rpc_factory(RPCFactory::Config{.directory_size = _cfg.max_recipes_count}) {}

public:
    /// Initialize the prompt
    void initialize() {
        assert(_dl);
        _dl->initialize();
    }

    /// Pulls messages from the datalink, extracts RPC information, invokes the RPC and writes back a returnstatus
    void update() {
        assert(_dl);

        _dl->pull(); // pull messages from the Datalink stream (such as UART) into it's.

        if (auto message = _dl->read_message()) {
            if (auto rpc = _rpc_factory.from_message(*message)) {
                auto rpc_result = rpc->invoke(); // do the remote procedure call

                if (const auto reply = OutgoingMessageFactory::from_rpc_result(std::move(rpc_result), message->module);
                    reply) {
                    _dl->write_message(*reply);
                }
            }
        }

        _dl->push(); // push out messages queued up in the
    }

    /// Set the active datalink used by the prompt
    void hotload_datalink(std::shared_ptr<Datalink> dl) { _dl.swap(dl); }

    /// Add an `RPCRecipe` to the prompt.
    void hotload_rpc_recipe(std::unique_ptr<RPCRecipe> recipe) {
        DBG("Prompt: Loading recipe: %s", std::string(recipe->module()).c_str());
        _rpc_factory.hotload_rpc_recipe(std::move(recipe));
    }

private:
    const Config _cfg;

    RPCFactory _rpc_factory;
    std::shared_ptr<Datalink> _dl = nullptr;
};

} // namespace kaskas::prompt
