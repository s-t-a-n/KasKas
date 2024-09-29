#pragma once

#include "kaskas/prompt/message/incoming_message_factory.hpp"
#include "kaskas/prompt/message/outgoing_message_factory.hpp"

#include <spine/core/utils/string.hpp>
#include <spine/io/stream/buffered_stream.hpp>
#include <spine/io/stream/transaction.hpp>

namespace kaskas::prompt {

class Request {
public:
private:
    using BufferedStream = spn::io::BufferedStream;
    spn::io::BufferedStream::Transaction _transaction;
};

class Datalink {
public:
    using BufferedStream = spn::io::BufferedStream;
    using Config = BufferedStream::Config;

public:
    Datalink(std::shared_ptr<spn::io::Stream> stream, BufferedStream::Config&& cfg)
        : _stream(std::move(stream), std::move(cfg)) {}

    Datalink(std::shared_ptr<spn::io::Stream> stream, const BufferedStream::Config& cfg)
        : Datalink(std::move(stream), BufferedStream::Config(cfg)) {}

    /// Initializes the datalink.
    void initialize() {}

    /// Pull data into the buffer from the datalink's active stream (such as UART). Returns amount of bytes pulled from
    /// stream.
    size_t pull() { return _stream.pull_in_data(); }

    /// Push data into the datalink's outgoing stream from the buffer. Returns amount of bytes pushed into stream.
    size_t push() { return _stream.push_out_data(); }

    using IError = IncomingMessageFactory::Error;

    /// Attempts to read a message from the buffer. Returns the message if successful, or an error code if not.
    spn::structure::Result<MessageWithStorage<BufferedStream::Transaction>, IError> read_message() {
        auto transaction = _stream.new_transaction();
        if (!transaction) return {};
        if (auto message = IncomingMessageFactory::from_view(transaction->incoming())) {
            return MessageWithStorage<BufferedStream::Transaction>(
                message.unwrap(), std::make_unique<BufferedStream::Transaction>(std::move(*transaction)));
        } else if (message.is_failed()) {
            return message.error_value();
        }
        return {};
    }

    using OError = OutgoingMessageFactory::Error;

    /// Write a message into the `Transaction` buffer. Returns the bytes written if succesful, or an errorcode if not.
    spn::structure::Result<size_t, OError> write_message(const Message& msg) {
        size_t bytes_written = 0;
        //    bytes_written += _stream.buffered_write(Dialect::REPLY_HEADER);

        bytes_written += _stream.buffered_write(msg.module);
        bytes_written += _stream.buffered_write(msg.operant);
        bytes_written += _stream.buffered_write(msg.cmd_or_status);
        if (msg.arguments) {
            bytes_written += _stream.buffered_write(std::string_view(":"));
            bytes_written += _stream.buffered_write(msg.arguments.value());
        }
        //    _stream.buffered_write(Dialect::REPLY_FOOTER);
        bytes_written += _stream.buffered_write(Dialect::REPLY_CRLF);

        return bytes_written;
    }

private:
    BufferedStream _stream;
};

} // namespace kaskas::prompt