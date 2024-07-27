#pragma once

// #include "kaskas/prompt/message_factory.hpp"

#include <optional>
#include <string_view>

namespace kaskas::prompt {

class IncomingMessageFactory;

/// Represents a message with module, operant, command, and arguments.
class Message {
public:
    Message(const Message& other) :
        module(other.module),
        operant(other.operant),
        cmd(other.cmd),
        arguments(other.arguments) {}
    Message(Message&& other) :
        module(std::move(other.module)),
        operant(std::move(other.operant)),
        cmd(std::move(other.cmd)),
        arguments(std::move(other.arguments)) {}
    Message& operator=(const Message& other) {
        if (this == &other)
            return *this;
        module = other.module;
        operant = other.operant;
        cmd = other.cmd;
        arguments = other.arguments;
        return *this;
    }
    Message& operator=(Message&& other) {
        if (this == &other)
            return *this;
        module = std::move(other.module);
        operant = std::move(other.operant);
        cmd = std::move(other.cmd);
        arguments = std::move(other.arguments);
        return *this;
    }
    Message(const std::string_view& module,
            const std::string_view& operant,
            const std::optional<std::string_view>& command = {},
            const std::optional<std::string_view>& arguments = {}) :
        module(module),
        operant(operant),
        cmd(command),
        arguments(arguments) {}

    std::string_view module;
    std::string_view operant;
    std::optional<std::string_view> cmd;
    std::optional<std::string_view> arguments;

    /// Returns the message as a string.
    [[nodiscard]] std::string as_string() const {
        std::string s{};
        s.reserve(module.size() + operant.size() + cmd.value_or("").size() + arguments.value_or("").size());

        s += module;
        s += operant;
        if (cmd) {
            s += *cmd;
            if (arguments) {
                s += Dialect::KV_SEPARATOR;
                s += *arguments;
            }
        }
        return s;
    }

protected:
    Message() {}

    friend IncomingMessageFactory;
};

template<typename T>
class MessageWithStorage : public Message {
public:
    MessageWithStorage(Message&& message, T&& storage) : Message(std::move(message)), _storage(std::move(storage)) {}

    const T& storage() const { return _storage; }

private:
    T _storage;
};

} // namespace kaskas::prompt
