#pragma once

// #include "kaskas/prompt/message_factory.hpp"

#include <memory>
#include <optional>
#include <string_view>

namespace kaskas::prompt {

class IncomingMessageFactory;

/// Represents a message with module, operant, command, and arguments.
class Message {
public:
    Message(const Message& other)
        : module(other.module), operant(other.operant), cmd_or_status(other.cmd_or_status), arguments(other.arguments) {
    }
    Message(Message&& other)
        : module(std::move(other.module)), operant(std::move(other.operant)),
          cmd_or_status(std::move(other.cmd_or_status)), arguments(std::move(other.arguments)) {}

    Message& operator=(const Message& other) {
        if (this == &other) return *this;
        module = other.module;
        operant = other.operant;
        cmd_or_status = other.cmd_or_status;
        arguments = other.arguments;
        return *this;
    }
    Message& operator=(Message&& other) {
        if (this == &other) return *this;
        module = std::move(other.module);
        operant = std::move(other.operant);
        cmd_or_status = std::move(other.cmd_or_status);
        arguments = std::move(other.arguments);
        return *this;
    }
    Message(const std::string_view& module, const std::string_view& operant,
            const std::optional<std::string_view>& command_or_status = {},
            const std::optional<std::string_view>& arguments = {})
        : module(module), operant(operant), cmd_or_status(command_or_status), arguments(arguments) {}
    ~Message() = default;

    std::string_view module;
    std::string_view operant;
    std::optional<std::string_view> cmd_or_status;
    std::optional<std::string_view> arguments;

    /// Returns the message as a string.
    [[nodiscard]] std::string as_string() const {
        std::string s{};

        auto reserved_length = module.size() + operant.size() + cmd_or_status.value_or("").size()
                               + 1 /* kvsep */ + arguments.value_or("").size() + 1 /* nullbyte */;
        s.reserve(reserved_length);
        reserved_length = s.capacity(); // for sanity checks below

        s += module;
        s += operant;
        if (cmd_or_status) {
            s += *cmd_or_status;
            if (arguments) {
                s += Dialect::KV_SEPARATOR;
                s += *arguments;
            }
        }
        assert(s.size() <= reserved_length); // single allocation
        assert(s.capacity() == reserved_length);
        return s;
    }
};

template<typename T>
class MessageWithStorage : public Message {
public:
    MessageWithStorage(Message&& message, std::unique_ptr<T> storage)
        : Message(std::move(message)), _storage(std::move(storage)) {}

    const T& storage() const {
        assert(_storage);
        return *_storage;
    }

private:
    std::unique_ptr<T> _storage;
};

} // namespace kaskas::prompt
