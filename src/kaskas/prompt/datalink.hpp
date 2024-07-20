#pragma once

#include "kaskas/prompt/charbuffer.hpp"
#include "kaskas/prompt/message.hpp"

#include <spine/core/debugging.hpp>
#include <spine/platform/hal.hpp>
#include <spine/structure/pool.hpp>

#include <optional>

namespace kaskas::prompt {

using spn::structure::Pool;

class Datalink {
public:
    struct Config {
        const size_t message_length;
        size_t pool_size;
    };

    Datalink(const Config&& cfg) : _cfg(cfg), _bufferpool(nullptr) {}

    void hotload_bufferpool(const std::shared_ptr<Pool<CharBuffer>>& bufferpool) { _bufferpool = bufferpool; }

    // todo migrate this to a proper Stream like class, so as to avoid the smelly stuff below
    virtual ~Datalink() = default;
    virtual void initialize() = 0;

    void send_message(const Message& msg) {
        write(Dialect::REPLY_HEADER);

        write(msg.module());
        write(msg.operant());

        if (msg.key()) {
            write(*msg.key());
        }
        if (msg.value()) {
            write(std::string_view(":"));
            write(*msg.value());
        }
        write(Dialect::REPLY_FOOTER);
        write(std::string_view("\n\r"));
    }

    std::optional<Message> receive_message() {
        constexpr auto return_carriers = "\r\n";

        if (available() == 0)
            return {};

        if (!_unfinished) {
            _unfinished = std::move(_bufferpool->acquire());
            assert(_unfinished);
            _unfinished->reset();
        }

        if (_unfinished->length >= _unfinished->capacity - 1) {
            DBG("Datalink: buffer is full");
        }

        // read to buffer
        assert(_unfinished->raw != nullptr);
        assert(_unfinished->capacity > 0);
        _unfinished->length += read(_unfinished->raw + _unfinished->length,
                                    _unfinished->capacity - _unfinished->length - 1); // 1 for nullbyte termination
        assert(_unfinished->length < _unfinished->capacity); // sanity check for read

        if (_unfinished->length == 0) // return when nothing has been read
            return {};

        _unfinished->raw[_unfinished->length] = '\0'; // null terminate  unconditionally

        // check if there is a partial line
        const auto nl = strcspn(_unfinished->raw, return_carriers);
        if (nl == _unfinished->length) {
            if (_unfinished->length >= _unfinished->capacity - Dialect::MINIMAL_CMD_LENGTH) {
                DBG("No CRLF found in entire buffer, discarding buffer.")
                _unfinished->reset();
            }
            return {};
        }

        // we have a partial line, recover it
        auto message_buffer = _bufferpool->acquire();
        if (!message_buffer)
            return {};
        message_buffer->reset();

        // copy in the discovered line to the message buffer
        message_buffer->length = nl + 1;
        assert(_unfinished->length >= message_buffer->length);
        strlcpy(message_buffer->raw, _unfinished->raw, message_buffer->length);

        // find all skipable characters
        const auto skip_length = [return_carriers](const char* str, size_t str_len) {
            size_t skip = 0;
            while (str_len > 0 && strcspn(str, return_carriers) == 0) {
                ++skip;
                ++str;
                --str_len;
            }
            return skip;
        };

        // move forward all that remains in buffer
        // todo: ringbuffer ?
        assert(_unfinished->length >= message_buffer->length);
        _unfinished->length -= message_buffer->length;

        const auto skip = skip_length(_unfinished->raw + message_buffer->length, _unfinished->length);
        assert(_unfinished->length >= skip);
        _unfinished->length -= skip;

        memmove(_unfinished->raw, _unfinished->raw + message_buffer->length + skip, _unfinished->length);
        _unfinished->raw[_unfinished->length] = '\0';

        // warning, todo: the following debug print needs to be cleared of newline characters, otherwise kaskaspython
        // does not understand what is line belongs to what
        // DBG("Returning message: {%s}, remaining(%i) in buffer: {%s}",
        //      message_buffer->raw,
        //      _unfinished->length,
        //      _unfinished->raw);
        // HAL::delay_ms(200);

        return Message::from_buffer(std::move(message_buffer));
    }

protected:
    size_t write(const std::string_view& view) { return write((uint8_t*)view.data(), view.length()); }
    size_t write(int8_t* buffer, size_t length) { return write((uint8_t*)buffer, length); };
    virtual size_t write(uint8_t* buffer, size_t length) { assert(!"Virtual base function called"); };

    size_t read(const int8_t* buffer, size_t length) { return read((uint8_t*)buffer, length); };
    size_t read(const char* buffer, size_t length) { return read((uint8_t*)buffer, length); };
    virtual size_t read(uint8_t* buffer, size_t length) { assert(!"Virtual base function called"); };

    virtual size_t available() { assert(!"Virtual base function called"); };

protected:
    Config _cfg;
    std::shared_ptr<Pool<CharBuffer>> _bufferpool;

    std::shared_ptr<CharBuffer> _unfinished;
};

class SerialDatalink final : public Datalink {
public:
    SerialDatalink(const Config&& cfg, HAL::UART&& uart) : Datalink(std::move(cfg)), _uart(uart){};
    ~SerialDatalink() override = default;

public:
    void initialize() override { assert(_bufferpool); }

    size_t available() override { return _uart.available(); }

    size_t write(uint8_t* buffer, size_t length) override { return _uart.write(buffer, length); }

    size_t read(uint8_t* buffer, size_t length) override { return _uart.read(buffer, length); }

private:
    HAL::UART _uart;
};

class MockDatalink final : public Datalink {
public:
    MockDatalink(const Config&& cfg) :
        Datalink(std::move(cfg)),
        _stdin(_cfg.pool_size),
        _stdout(_cfg.pool_size),
        _active_out(&_stdout),
        _active_in(&_stdin){};
    ~MockDatalink() override = default;

public:
    void initialize() override { assert(_bufferpool); }

    void inject_message(const Message& msg) {
        swap_streams();
        send_message(msg);
        swap_streams();
    }

    void inject_raw_string(const std::string& raw_string) {
        swap_streams();
        write((uint8_t*)(raw_string.c_str()), raw_string.size());
        swap_streams();
    }

    std::optional<Message> extract_reply() {
        swap_streams();
        auto m = receive_message();
        swap_streams();
        return std::move(m);
    }
    std::shared_ptr<Pool<CharBuffer>>& bufferpool() { return _bufferpool; }

protected:
    using Vector = spn::structure::Vector<std::string>;

    void swap_streams() {
        assert(_active_in != nullptr);
        assert(_active_out != nullptr);
        const auto temp = _active_in;
        _active_in = _active_out;
        _active_out = temp;
    }

    size_t write(uint8_t* buffer, size_t length) override {
        std::string s((char*)buffer, length);
        if (length == 0 || _active_out->full())
            return 0;
        _active_out->push_back(s);
        return length;
    }

    size_t read(uint8_t* buffer, size_t length) override {
        size_t popped = 0;
        size_t offset = 0;
        for (const auto& s : *_active_in) {
            if (s.length() <= length - offset) {
                memmove(buffer + offset, s.data(), s.length());
                offset += s.length();
                // DBG("s.length : %i", s.length());
                // DBG("'%s'", s.c_str());
                ++popped;
            } else {
                break;
            }
        }
        for (int i = 0; i < popped; ++i) {
            const auto _ = _active_in->pop_front();
        }

        // const auto s = std::string((char*)buffer, offset);
        // DBG("len: %i, s: '%s'", offset, s.c_str());

        // assert(offset == 15);
        return offset;
    }

    size_t available() override {
        size_t bytes_available = 0;
        for (const auto& s : *_active_in) {
            bytes_available += s.length();
        }
        return bytes_available;
    }

private:
    Vector _stdin;
    Vector _stdout;

    Vector* _active_in;
    Vector* _active_out;
};

} // namespace kaskas::prompt
