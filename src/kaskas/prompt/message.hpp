#pragma once
#include "kaskas/prompt/dialect.hpp"
#include "kaskas/prompt/rpc_result.hpp"

#include <spine/core/assert.hpp>
#include <spine/core/debugging.hpp>

#include <AH/STL/memory>
#include <cstring>
#include <optional>
#include <string_view>

namespace kaskas::prompt {
class Message {
public:
    /*
     * The following patterns are legal messages:
     * CMD=             : list all variables than can be set for CMD
     * CMD?             : list all variable that can be requested for CMD
     * CMD!             : list all functions that can be called for CMD
     * CMD=var:1        : set a variable
     * CMD?var          : request a variable
     * CMD!function     : function call
     * CMD<arguments    : reply to request
     */

    const std::string_view& cmd() const {
        assert(_cmd != std::string_view{});
        return _cmd;
    }
    const std::string_view& operant() const {
        assert(_operant != std::string_view{});
        return _operant;
    }

    const std::optional<std::string_view>& key() const { return _key; }
    const std::optional<std::string_view>& value() const { return _value; }

    static std::optional<Message> from_buffer(std::shared_ptr<CharBuffer>&& buffer) {
        assert(buffer->capacity > 0);

        // ensure null termination
        buffer->raw[buffer->capacity - 1] = '\0';
        const auto operant_idx = strcspn(buffer->raw, Dialect::OPERANTS);

        DBGF("buffer: %s, length: %i, operant @ %i", static_cast<const char*>(buffer->raw), buffer->length,
             operant_idx);
        if (operant_idx == buffer->length - 1) {
            // no operant found
            return std::nullopt;
        }
        if (operant_idx < Dialect::MINIMAL_CMD_LENGTH || operant_idx > Dialect::MAXIMAL_CMD_LENGTH) {
            // command size is too small or too big
            return std::nullopt;
        }

        Message m;
        m._cmd = std::string_view(buffer->raw, operant_idx);
        m._operant = std::string_view(buffer->raw + operant_idx, 1);
        assert(buffer->length >= operant_idx + 1);
        bool newline_at_end = buffer->raw[buffer->length - 1] == '\n';
        m._value = std::string_view(buffer->raw + operant_idx + 1, buffer->length - operant_idx - 1 - newline_at_end);
        m._buffer = std::move(buffer);
        return m;
    }

    static std::optional<Message> from_result(std::shared_ptr<CharBuffer>&& buffer, const RPCResult& result,
                                              const std::string_view& cmd) {
        assert(buffer);
        assert(buffer->capacity > 0);

        Message m;

        // m._cmd = cmd;
        // m._operant = std::string_view(Dialect::OPERANT_REPLY, 1);
        // m.argument()

        m._buffer = std::move(buffer);

        // add command
        auto head = m._buffer->raw;
        strncpy(head, cmd.data(), cmd.length());

        // add '<'
        head += cmd.length();
        strncpy(head, Dialect::OPERANT_REPLY, 1);

        // add integer status
        const auto status_str = result.status ? std::to_string(*result.status) : std::string("0");
        head += 1;
        strncpy(head, status_str.c_str(), 1);

        if (result.return_value) {
            // add separator
            head += 1;
            strncpy(head, ":", 1);

            // add return value
            head += 1;
            strncpy(head, result.return_value->c_str(), result.return_value->length());
        }
        // add nullbyte
        head += 1;
        *head = '\0';

        m._buffer->length = head - m._buffer->raw;

        return std::move(m);
    }

    // protected:
    // Message() = default;

private:
    std::string_view _cmd;
    std::string_view _operant;
    std::optional<std::string_view> _key;
    std::optional<std::string_view> _value;

    std::shared_ptr<CharBuffer> _buffer;
};
} // namespace kaskas::prompt