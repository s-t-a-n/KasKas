#pragma once

#include "kaskas/prompt/datalink.hpp"
#include "kaskas/prompt/dialect.hpp"
#include "kaskas/prompt/incoming_message_factory.hpp"
#include "kaskas/prompt/message.hpp"
#include "kaskas/prompt/outgoing_message_factory.hpp"
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

namespace detail {

inline void send_message(spn::io::BufferedStream::Transaction& transaction, const MessageWithStorage<RPCResult>& msg) {
    transaction.outgoing(Dialect::REPLY_HEADER);

    transaction.outgoing(msg.module);
    transaction.outgoing(msg.operant);

    if (msg.cmd) {
        transaction.outgoing(msg.cmd.value());
    }
    if (msg.arguments) {
        // DBG("before sending: {%s}", std::string(msg.arguments.value()).c_str());
        // DBG("before sending storage: {%s}", std::string(msg.storage().return_value.value()).c_str());
        transaction.outgoing(std::string_view(":"));
        transaction.outgoing(msg.arguments.value());
    }
    transaction.outgoing(Dialect::REPLY_FOOTER);
    transaction.outgoing(std::string_view("\r\n"));
}

} // namespace detail

class Prompt {
public:
    struct Config {
        size_t io_buffer_size; // size of input and output buffer
        const std::string_view line_delimiters = "\r\n"; // delimiters to split input with
    };

    Prompt(const Config&& cfg) : _cfg(cfg), _rpc_factory(RPCFactory::Config{.directory_size = 16}) {}

public:
    void initialize() {
        assert(_dl);
        _dl->initialize();
    }

    // void attach() { dbg::enable_dbg_output(false); }
    // void detach() { dbg::enable_dbg_output(true); }

    void update() {
        assert(_dl);

        _dl->pull();

        if (auto transaction = _dl->incoming_transaction()) {
            // DBG("Prompt: New transaction coming over the wire: {%s}", std::string(transaction->incoming()).c_str());
            if (auto message = IncomingMessageFactory::from_view(transaction->incoming())) {
                if (const auto rpc = _rpc_factory.from_message(*message)) {
                    auto res = rpc->invoke();

                    // DBG("invoke: res: {%s}", res.return_value->c_str());

                    if (const auto reply = OutgoingMessageFactory::from_result(std::move(res), message->module);
                        reply) {
                        detail::send_message(*transaction, *reply);
                    }
                } else {
                    DBG("Prompt: Couldnt build RPC from message: {%s}", message->as_string().c_str());
                    if (const auto reply = OutgoingMessageFactory::from_result(
                            RPCResult(std::move(message->as_string()), RPCResult::Status::BAD_INPUT),
                            message->module))
                        detail::send_message(*transaction, *reply);
                }
            }
            transaction->commit();
        }

        // if (const auto message = _dl->receive_message()) {
        //     DBG("update: received message: {%s}", message->as_string().c_str());
        //     if (const auto rpc = _rpc_factory.from_message(*message)) {
        //         // DBG("update: invoking for: {%s}", message->as_string().c_str());
        //         auto res = rpc->invoke();
        //         if (const auto reply = OutgoingMessageFactory::from_result(std::move(res), message->module); reply)
        //             _dl->send_message(*reply);
        //     } else {
        //         DBG("Prompt: Couldnt build RPC from message: {%s}", message->as_string().c_str());
        //         if (const auto reply = OutgoingMessageFactory::from_result(
        //                 RPCResult(message->as_string(), RPCResult::Status::BAD_INPUT),
        //                 message->module))
        //             _dl->send_message(*reply);
        //     }
        // }

        _dl->push();
    }

    void hotload_datalink(std::shared_ptr<Datalink> dl) { _dl.swap(dl); }

    void hotload_rpc_recipe(std::unique_ptr<RPCRecipe> recipe) {
        const auto cmd_str = std::string(recipe->module());
        DBG("Prompt: Loading recipe: %s", cmd_str.c_str());
        _rpc_factory.hotload_rpc_recipe(std::move(recipe));
    }

private:
    const Config _cfg;

    RPCFactory _rpc_factory;

    std::shared_ptr<Datalink> _dl;
};

} // namespace kaskas::prompt
