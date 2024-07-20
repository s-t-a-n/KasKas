#pragma once
#include "kaskas/prompt/charbuffer.hpp"
#include "kaskas/prompt/dialect.hpp"
#include "kaskas/prompt/rpc_result.hpp"

#include <spine/core/debugging.hpp>

#include <cstring>
#include <optional>
#include <string_view>

namespace kaskas::prompt {
class Message {
public:
    enum class Type {};

public:
    Message() = default;
    Message(Message&& other) noexcept :
        _module(std::move(other._module)),
        _operant(std::move(other._operant)),
        _cmd(std::move(other._cmd)),
        _arguments(std::move(other._arguments)),
        _buffer(std::move(other._buffer)) {}
    Message& operator=(Message& other) = delete;
    Message& operator=(Message&& other) noexcept {
        if (this == &other)
            return *this;
        _module = std::move(other._module);
        _operant = std::move(other._operant);
        _cmd = std::move(other._cmd);
        _arguments = std::move(other._arguments);
        _buffer = std::move(other._buffer);
        return *this;
    }

public:
    const std::shared_ptr<CharBuffer> buffer() const {
        assert(_buffer);
        return _buffer;
    }

    const std::string_view& module() const {
        assert(_module != std::string_view{});
        return _module;
    }
    const std::string_view& operant() const {
        assert(_operant != std::string_view{});
        return _operant;
    }

    const std::optional<std::string_view>& key() const { return _cmd; }
    const std::optional<std::string_view>& value() const { return _arguments; }

    static std::optional<Message> from_buffer(std::shared_ptr<CharBuffer> buffer) {
        assert(buffer && buffer->capacity > 0);
        buffer->raw[buffer->capacity - 1] = '\0'; // ensure null termination

        if (memcmp(buffer->raw, Dialect::OPERANT_PRINT_USAGE, 2) == 0) { // user request to dump all possible commands
            Message m;
            m._module = Dialect::OPERANT_PRINT_USAGE;
            m._operant = Dialect::OPERANT_PRINT_USAGE;
            return m;
        }

        const auto operant_idx = strcspn(buffer->raw, Dialect::OPERANTS.data());
        if (operant_idx == buffer->length - 1) { // no operant found
            return std::nullopt;
        }
        if (operant_idx < Dialect::MINIMAL_CMD_LENGTH
            || operant_idx > Dialect::MAXIMAL_CMD_LENGTH) { // command size is too small or too big
            return std::nullopt;
        }

        bool newline_at_end = buffer->raw[buffer->length - 1] == '\n';

        Message m;

        // add module
        auto head = buffer->raw;
        m._module = std::string_view(head, operant_idx);

        // add operant
        head += operant_idx;
        m._operant = std::string_view(head, 1);

        // extract command and arguments
        head += 1;
        const auto kv_idx = strcspn(head, Dialect::KV_SEPARATOR);
        if (kv_idx < buffer->length - (head - buffer->raw)) {
            // key/value separator found
            m._cmd = std::string_view(head, kv_idx);
            head += kv_idx + 1; // skip separator
            const auto arguments_length = buffer->length - (head - buffer->raw) - newline_at_end;
            m._arguments =
                arguments_length > 0 ? std::make_optional(std::string_view(head, arguments_length)) : std::nullopt;
        } else {
            m._cmd = std::string_view(head, buffer->length - (head - buffer->raw) - newline_at_end);
            m._arguments = std::nullopt;
        }

        m._buffer = std::move(buffer);
        return std::move(m);
    }

    static std::optional<Message>
    from_result(std::shared_ptr<CharBuffer>&& buffer, const RPCResult& result, const std::string_view& module) {
        assert(buffer);
        assert(buffer->capacity > 0);

        Message m;

        m._buffer = std::move(buffer);

        // add module
        auto head = m._buffer->raw;
        strncpy(head, module.data(), module.length());
        m._module = std::string_view(head, module.length());

        // add operator signifying a reply '<'
        head += module.length();
        strncpy(head, Dialect::OPERANT_REPLY, 1);
        m._operant = std::string_view(head, 1);

        // add integer status
        const auto status_str = std::to_string(magic_enum::enum_integer(result.status));
        head += status_str.length();
        strncpy(head, status_str.c_str(), status_str.length());
        m._cmd = std::string_view(head, status_str.length());

        // add arguments
        if (result.return_value) {
            // add separator
            head += 1;
            strncpy(head, ":", 1);

            // add return value
            head += 1;
            strncpy(head, result.return_value->c_str(), result.return_value->length());
            m._arguments = std::string_view(head, result.return_value->length());
            head += result.return_value->length();
        }
        head[1] = '\0'; // add nullbyte
        m._buffer->length = head - m._buffer->raw;

        return std::move(m);
    }

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

    std::shared_ptr<CharBuffer> _buffer;
};

// todo: clean this up and migrate this to a message factory
} // namespace kaskas::prompt
