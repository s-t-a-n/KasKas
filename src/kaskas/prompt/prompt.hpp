#pragma once

#include "kaskas/prompt/datalink.hpp"
#include "kaskas/prompt/dialect.hpp"
#include "kaskas/prompt/message.hpp"
#include "kaskas/prompt/rpc.hpp"

#include <magic_enum/magic_enum.hpp>
#include <magic_enum/magic_enum_all.hpp>
#include <spine/core/assert.hpp>
#include <spine/core/debugging.hpp>
#include <spine/platform/hal.hpp>
#include <spine/structure/list.hpp>

#include <AH/STL/memory>
#include <cstring>
#include <initializer_list>
#include <optional>
#include <string_view>
#include <variant>

namespace kaskas::prompt {

class Prompt {
public:
    struct Config {
        size_t message_length;
        size_t pool_size;
    };

    Prompt(const Config&& cfg)
        : _cfg(cfg), _rpc_factory(RPCFactory::Config{.directory_size = 16}),
          _bufferpool(std::make_shared<Pool<CharBuffer>>(_cfg.pool_size)) {
        for (int i = 0; i < _cfg.pool_size; ++i) {
            _bufferpool->populate(new CharBuffer(_cfg.message_length));
        }
        assert(_bufferpool->is_fully_populated());
    }

public:
    void initialize() { _dl->initialize(); }
    void update() {
        if (const auto message = _dl->receive_message()) {
            DBGF("received message");
            if (const auto rpc = _rpc_factory.from_message(*message)) {
                DBGF("built rpc from message");
                const auto res = rpc->invoke();
                DBGF("hello");
                auto cb = _bufferpool->acquire();
                assert(cb);
                assert(cb->raw);
                const auto reply = Message::from_result(std::move(cb), res, message->cmd());
                DBGF("hello2");

                const auto c = std::string(reply->cmd());
                const auto o = std::string(reply->operant());
                const auto a = std::string(reply->value());
                DBGF("replying: %s:%s:%s", c.c_str(), o.c_str(), a.c_str());
            } else {
                DBGF("Couldnt built rpc from message");
            }
        }
    }

    void hotload_datalink(std::unique_ptr<Datalink> dl) {
        assert(_dl == nullptr);
        _dl.swap(dl);
        assert(dl == nullptr);

        assert(_bufferpool.get() != nullptr);
        _dl->hotload_bufferpool(_bufferpool);
    }
    void hotload_rpc_recipe(std::unique_ptr<RPCRecipe> recipe) { _rpc_factory.hotload_rpc_recipe(std::move(recipe)); }

private:
    const Config _cfg;

    RPCFactory _rpc_factory;

    std::unique_ptr<Datalink> _dl;
    std::shared_ptr<Pool<CharBuffer>> _bufferpool;
};

} // namespace kaskas::prompt