#pragma once

#include "kaskas/prompt/datalink.hpp"
#include "kaskas/prompt/dialect.hpp"
#include "kaskas/prompt/message.hpp"
#include "kaskas/prompt/rpc.hpp"

#include <magic_enum/magic_enum.hpp>
#include <magic_enum/magic_enum_all.hpp>
#include <spine/core/debugging.hpp>
#include <spine/platform/hal.hpp>
#include <spine/structure/list.hpp>

#include <cstring>
#include <initializer_list>
#include <memory>
#include <optional>
#include <string_view>
#include <variant>

namespace kaskas::prompt {

class Prompt {
public:
    struct Config {
        size_t message_length;
        size_t pool_size;
        size_t input_buffer_size;
        const std::string_view line_delimiters = "\r\n";
    };

    Prompt(const Config&& cfg) :
        _cfg(cfg),
        _rpc_factory(RPCFactory::Config{.directory_size = 16}),
        _bufferpool(std::make_shared<Pool<StaticString>>(_cfg.pool_size)) {
        for (int i = 0; i < _cfg.pool_size; ++i) {
            _bufferpool->populate(std::make_shared<StaticString>(_cfg.message_length));
        }
        assert(_bufferpool->is_fully_populated());
    }

public:
    void initialize() {
        assert(_dl);
        _dl->initialize();
    }

    void attach() { dbg::enable_dbg_output(false); }
    void detach() { dbg::enable_dbg_output(true); }

    void update() {
        assert(_dl);
        if (const auto message = _dl->receive_message()) {
            DBG("update: received message: {%s}", message->as_string().c_str());
            if (const auto rpc = _rpc_factory.from_message(*message)) {
                // DBG("update: invoking for: {%s}", message->as_string().c_str());
                const auto res = rpc->invoke();
                if (const auto reply = Message::create_from_result(_bufferpool->acquire(), res, message->module()))
                    _dl->send_message(*reply);
            } else {
                DBG("Prompt: Couldnt build RPC from message: {%s}", message->as_string().c_str());
                if (const auto reply =
                        Message::create_from_result(_bufferpool->acquire(),
                                                    RPCResult(message->as_string(), RPCResult::Status::BAD_INPUT),
                                                    message->module()))
                    _dl->send_message(*reply);
            }
        }
    }

    void hotload_datalink(std::shared_ptr<Datalink> dl) {
        assert(_dl == nullptr);
        _dl.swap(dl);
        assert(dl == nullptr);
        assert(_bufferpool != nullptr);
        _dl->hotload_bufferpool(_bufferpool);
    }
    void hotload_rpc_recipe(std::unique_ptr<RPCRecipe> recipe) {
        const auto cmd_str = std::string(recipe->command());
        DBG("Prompt: Loading recipe: %s", cmd_str.c_str());
        _rpc_factory.hotload_rpc_recipe(std::move(recipe));
    }

private:
    const Config _cfg;

    RPCFactory _rpc_factory;

    std::shared_ptr<Datalink> _dl;
    std::shared_ptr<Pool<StaticString>> _bufferpool;
};

} // namespace kaskas::prompt
