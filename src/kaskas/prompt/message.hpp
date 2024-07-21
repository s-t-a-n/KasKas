#pragma once

#include "kaskas/prompt/StaticString.hpp"
#include "kaskas/prompt/message_factory.hpp"

#include <optional>
#include <string_view>

namespace kaskas::prompt {

/// Represents a message with module, operant, command, and arguments.
class Message {
public:
    enum class Type {};

public:
    Message() = default;
    Message(Message&& other) noexcept = default;
    Message& operator=(Message&& other) noexcept = default;

    /// Returns the buffer associated with the message.
    const std::shared_ptr<StaticString> buffer() const {
        assert(_buffer);
        return _buffer;
    }

    /// Returns the module associated with the message.
    const std::string_view& module() const {
        assert(_module != std::string_view{});
        return _module;
    }

    /// Returns the operant associated with the message.
    const std::string_view& operant() const {
        assert(_operant != std::string_view{});
        return _operant;
    }

    /// Returns the command key associated with the message.
    const std::optional<std::string_view>& key() const { return _cmd; }

    /// Returns the command arguments associated with the message.
    const std::optional<std::string_view>& value() const { return _arguments; }

    /// Constructs a Message object from a buffer if valid.
    static std::optional<Message> create_from_buffer(std::shared_ptr<StaticString> buffer) {
        MessageFactory factory;
        return factory.from_buffer(buffer);
    }

    /// Constructs a Message object from an RPC result and a buffer.
    static std::optional<Message> create_from_result(std::shared_ptr<StaticString>&& buffer,
                                                     const RPCResult& result,
                                                     const std::string_view& module) {
        MessageFactory factory;
        return factory.from_result(std::move(buffer), result, module);
    }

    /// Returns the message as a string.
    [[nodiscard]] std::string as_string() const {
        std::string s;
        s += module();
        s += operant();
        if (key()) {
            s += *key();
            if (value()) {
                s += Dialect::KV_SEPARATOR;
                s += *value();
            }
        }
        return s;
    }

private:
    std::string_view _module;
    std::string_view _operant;
    std::optional<std::string_view> _cmd;
    std::optional<std::string_view> _arguments;

    std::shared_ptr<StaticString> _buffer;

    friend MessageFactory;
};

} // namespace kaskas::prompt
