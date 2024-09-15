#pragma once

#include "kaskas/prompt/incoming_message_factory.hpp"
#include "kaskas/prompt/outgoing_message_factory.hpp"

#include <spine/core/utils/string.hpp>
#include <spine/io/stream/buffered_stream.hpp>
#include <spine/io/stream/transaction.hpp>

namespace kaskas::prompt {

// template<typename ResultType>
// struct Request {
//     spn::io::Transaction transaction;
//
//     std::optional<Message> receive_message() {
//         const auto view = transaction.incoming();
//         return IncomingMessageFactory::from_view(view);
//     }
//
//     void send_reply(MessageWithStorage<ResultType> msg) {
//         transaction.outgoing(Dialect::REPLY_HEADER);
//
//         transaction.outgoing(msg.module());
//         transaction.outgoing(msg.operant());
//
//         if (msg.key()) {
//             transaction.outgoing(*msg.key());
//         }
//         if (msg.value()) {
//             transaction.outgoing(std::string_view(":"));
//             transaction.outgoing(msg->value);
//         }
//         transaction.outgoing(Dialect::REPLY_FOOTER);
//         transaction.outgoing(std::string_view("\r\n"));
//
//         transaction.commit();
//     }
// };

class Datalink {
public:
    using BufferedStream = spn::io::BufferedStream;
    using Config = BufferedStream::Config;

public:
    Datalink(std::shared_ptr<spn::io::Stream> stream, BufferedStream::Config&& cfg)
        : _stream(std::move(stream), std::move(cfg)) {}

    Datalink(std::shared_ptr<spn::io::Stream> stream, const BufferedStream::Config& cfg)
        : Datalink(std::move(stream), BufferedStream::Config(cfg)) {}

    void initialize() {}

    /// Pull data from the datalink's stream. Returns amount of bytes pulled from stream.
    size_t pull() { return _stream.pull_in_data(); }

    /// Push data into the datalink's stream. Returns amount of bytes pushed into stream.
    size_t push() { return _stream.push_out_data(); }

    /// Returns a `Transaction` if it is available, or `std::nullopt` if it's not.
    std::optional<BufferedStream::Transaction> incoming_transaction() { return _stream.new_transaction(); }

private:
    BufferedStream _stream;
};

} // namespace kaskas::prompt