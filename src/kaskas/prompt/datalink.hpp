#pragma once

#include "kaskas/prompt/incoming_message_factory.hpp"
#include "kaskas/prompt/outgoing_message_factory.hpp"

#include <spine/core/utils/string.hpp>
#include <spine/io/stream/buffered_stream.hpp>
#include <spine/io/stream/transaction.hpp>
namespace kaskas::prompt {

template<typename ResultType>
struct Request {
    spn::io::Transaction transaction;
    std::optional<Message> receive_message() {
        const auto view = transaction.incoming();
        return IncomingMessageFactory::from_view(view);
    }
    void send_reply(MessageWithStorage<ResultType> msg) {
        transaction.outgoing(Dialect::REPLY_HEADER);

        transaction.outgoing(msg.module());
        transaction.outgoing(msg.operant());

        if (msg.key()) {
            transaction.outgoing(*msg.key());
        }
        if (msg.value()) {
            transaction.outgoing(std::string_view(":"));
            transaction.outgoing(msg->value);
        }
        transaction.outgoing(Dialect::REPLY_FOOTER);
        transaction.outgoing(std::string_view("\r\n"));

        transaction.commit();
    }
};

class Datalink {
public:
    using BufferedStream = spn::io::BufferedStream;
    using Config = BufferedStream::Config;

public:
    Datalink(std::shared_ptr<spn::io::Stream> stream, BufferedStream::Config&& cfg) :
        _stream(std::move(stream), std::move(cfg)) {}
    void initialize() {}

    // void send_message(const Message& msg) {
    //     _stream.buffered_write(Dialect::REPLY_HEADER);
    //
    //     _stream.buffered_write(msg.module);
    //     _stream.buffered_write(msg.operant);
    //
    //     if (msg.cmd) {
    //         _stream.buffered_write(*msg.cmd);
    //     }
    //     if (msg.arguments) {
    //         _stream.buffered_write(std::string_view(":"));
    //         _stream.buffered_write(*msg.arguments);
    //     }
    //     _stream.buffered_write(Dialect::REPLY_FOOTER);
    //     _stream.buffered_write(std::string_view("\r\n"));
    // }
    // void send_message(const MessageWithStorage<RPCResult>& msg) {

    // }

    size_t pull() { return _stream.pull_in_data(); }
    size_t push() { return _stream.push_out_data(); }

    std::optional<BufferedStream::Transaction> incoming_transaction() { return _stream.new_transaction(); }

    // std::optional<Message> receive_message() {
    //     // read bytes into linebuffer
    //     // uint8_t last_char = '\0';
    //     // while (available() > 0) {
    //     //     // DBG("receive_message: readingloop: available: %i", available());
    //     //
    //     //     // early break when a delimiter has been found, as to lose no input if possible
    //     //     if (_input_buffer.full()) {
    //     //         if (_input_buffer.delimiters().find(last_char) != std::string::npos)
    //     //             break; // break when the last incoming was a delimiter
    //     //         if (_input_buffer.overrun_space() == 0 && _input_buffer.has_line())
    //     //             break; // break when the buffer just turned full and the buffer already contains a line
    //     //     }
    //     //
    //     //     if (!read(last_char)) {
    //     //         return std::nullopt; // read failure
    //     //     }
    //     //     _input_buffer.push(last_char, true);
    //     // }
    //
    //     // DBG("receive_message: used space: %i", _input_buffer.used_space());
    //
    //     if (auto discovered_length = _stream.length_of_next_line(); discovered_length > 0) {
    //         // DBG("receive_message: has line");
    //
    //         // HAL::print("receive_message: has line: [ ");
    //         // for (int i = 0; i < _input_buffer.used_space(); ++i) {
    //         //     char v;
    //         //     _input_buffer.peek_at(v, i);
    //         //     HAL::print((unsigned char)v);
    //         //     HAL::print(", ");
    //         // }
    //         // HAL::println(" ]");
    //
    //         auto view = _stream.get_next_line_view(discovered_length);
    //         assert(view.has_value()); // guarded by if condition above
    //
    //         DBG("discovered length: %i,cb->size:%i, cb: {%s}",
    //             discovered_length,
    //             view->size(),
    //             std::string(*view).c_str());
    //
    //         return IncomingMessageFactory::from_view(*view);
    //     }
    //
    //     return {};
    // }

private:
    BufferedStream _stream;
};

} // namespace kaskas::prompt