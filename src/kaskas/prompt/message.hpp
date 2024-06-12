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

        // DBGF("buffer: %s, length: %i, operant @ %i", static_cast<const char*>(buffer->raw), buffer->length,
        // operant_idx);
        if (operant_idx == buffer->length - 1) {
            // no operant found
            return std::nullopt;
        }
        if (operant_idx < Dialect::MINIMAL_CMD_LENGTH || operant_idx > Dialect::MAXIMAL_CMD_LENGTH) {
            // command size is too small or too big
            return std::nullopt;
        }

        bool newline_at_end = buffer->raw[buffer->length - 1] == '\n';

        Message m;
        auto head = buffer->raw;
        m._cmd = std::string_view(head, operant_idx);

        head += operant_idx;
        m._operant = std::string_view(head, 1);

        head += 1;
        const auto kv_idx = strcspn(head, Dialect::KV_SEPARATOR);
        if (kv_idx < buffer->length - (head - buffer->raw)) {
            // key/value separator found
            m._key = std::string_view(head, kv_idx);
            head += kv_idx + 1; // skip separator
            m._value = std::string_view(head, buffer->length - (head - buffer->raw) - newline_at_end);
        } else {
            // const auto length = buffer->length - (head - buffer->raw) - newline_at_end;
            // DBGF("length: %i", length);
            // assert(length == 10);
            m._key = std::string_view(head, buffer->length - (head - buffer->raw) - newline_at_end);
            const auto s = std::string(*m._key);

            m._value = std::nullopt;
        }

        // DBGF("total length: %i", buffer->length);
        // const auto s1 = std::string(m.cmd());
        // const auto s2 = std::string(m.operant());
        // const auto s3 = std::string(*m.key());
        // DBGF("from buffer: %s:%s:%s", s1.c_str(), s2.c_str(), s3.c_str());
        // assert(m.key()->length() == 10);

        // assert(buffer->length >= operant_idx + 1);
        // m._value = std::string_view(buffer->raw + operant_idx + 1, buffer->length - operant_idx - 1 -
        // newline_at_end);
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
        m._cmd = std::string_view(head, cmd.length());

        // add '<'
        head += cmd.length();
        strncpy(head, Dialect::OPERANT_REPLY, 1);
        m._operant = std::string_view(head, 1);

        // add integer status
        const auto status_str = result.status ? std::to_string(*result.status) : std::string("0");
        head += status_str.length();
        strncpy(head, status_str.c_str(), status_str.length());
        m._key = std::string_view(head, status_str.length());

        if (result.return_value) {
            // add separator
            head += 1;
            strncpy(head, ":", 1);

            // add return value
            head += 1;
            strncpy(head, result.return_value->c_str(), result.return_value->length());
            m._value = std::string_view(head, result.return_value->length());
            head += result.return_value->length();
        }
        // add nullbyte
        head[1] = '\0';

        m._buffer->length = head - m._buffer->raw;

        DBGF("return value: '%s'", result.return_value->c_str());
        DBGF("resulting reply: %s", m.as_string().c_str());

        return std::move(m);
    }

    [[nodiscard]] std::string as_string() const {
        std::string s;
        s += cmd();
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