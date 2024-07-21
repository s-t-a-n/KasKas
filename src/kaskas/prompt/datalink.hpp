#pragma once

#include "kaskas/prompt/StaticString.hpp"
#include "kaskas/prompt/message.hpp"

#include <spine/core/debugging.hpp>
#include <spine/platform/hal.hpp>
#include <spine/structure/linebuffer.hpp>
#include <spine/structure/pool.hpp>

#include <memory>
#include <optional>

namespace kaskas::prompt {

using spn::structure::Pool;

class Transaction {
public:
private:
};

class IncomingTransaction : public Transaction {
public:
private:
};

class OutgoingTransaction : public Transaction {
public:
private:
};

class Datalink {
public:
    struct Config {
        size_t message_length; // maximum length of a single message
        size_t pool_size; // amount of messages available for pickup
        size_t buffer_size; // buffer size in bytes
        std::string_view delimiters = "\r\n";
    };

    Datalink(const Config&& cfg) : _cfg(cfg), _bufferpool(nullptr), _input_buffer(_cfg.buffer_size, _cfg.delimiters) {}

    /// Load a bufferpool after construction, allowing seperation of construction and initialization
    void hotload_bufferpool(const std::shared_ptr<Pool<StaticString>>& bufferpool) { _bufferpool = bufferpool; }

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
        write(std::string_view("\r\n"));
    }

    std::optional<Message> receive_message() {
        // constexpr auto return_carriers = "\r\n";

        // if (available() == 0)
        //     return {};
        //
        // if (!_unfinished) {
        //     _unfinished = std::move(_bufferpool->acquire());
        //     assert(_unfinished);
        //     _unfinished->reset();
        // }
        //
        // if (_unfinished->length >= _unfinished->capacity - 1) {
        //     DBG("Datalink: buffer is full");
        // }

        // if (available())
        // DBG("receive_message: available: %i", available());

        // read bytes into linebuffer
        uint8_t last_char = '\0';
        while (available() > 0) {
            // DBG("receive_message: readingloop: available: %i", available());

            // early break when a delimiter has been found, as to lose no input if possible
            if (_input_buffer.full()) {
                if (_input_buffer.delimiters().find(last_char) != std::string::npos)
                    break; // break when the last incoming was a delimiter
                if (_input_buffer.overrun_space() == 0 && _input_buffer.has_line())
                    break; // break when the buffer just turned full and the buffer already contains a line
            }

            if (!read(last_char)) {
                return std::nullopt; // read failure
            }
            _input_buffer.push(last_char, true);
        }

        // DBG("receive_message: used space: %i", _input_buffer.used_space());

        if (auto discovered_length = _input_buffer.length_of_next_line(); discovered_length != _input_buffer.npos()) {
            // DBG("receive_message: has line");

            HAL::print("receive_message: has line: [ ");
            for (int i = 0; i < _input_buffer.used_space(); ++i) {
                char v;
                _input_buffer.peek_at(v, i);
                HAL::print((unsigned char)v);
                HAL::print(", ");
            }
            HAL::println(" ]");

            if (discovered_length > _cfg.message_length) {
                DBG("Datalink: received message is too long (limit: %u), rejecting message with length {%u}",
                    _cfg.message_length,
                    discovered_length);
                _input_buffer.drop_next_line(discovered_length);
                return {};
            }

            auto cb = _bufferpool->acquire();
            cb->set_size(_input_buffer.get_next_line(cb->data(discovered_length), cb->capacity(), discovered_length));
            DBG("discovered length: %i,cb->size:%i, cb: {%s}", discovered_length, cb->size(), cb->c_str());

            return Message::create_from_buffer(std::move(cb));
        }

        return {};

        // // read to buffer
        // assert(_unfinished->raw != nullptr);
        // assert(_unfinished->capacity > 0);
        // _unfinished->length += read(_unfinished->raw + _unfinished->length,
        //                             _unfinished->capacity - _unfinished->length - 1); // 1 for nullbyte termination
        // assert(_unfinished->length < _unfinished->capacity); // sanity check for read
        //
        // if (_unfinished->length == 0) // return when nothing has been read
        //     return {};
        //
        // _unfinished->raw[_unfinished->length] = '\0'; // null terminate  unconditionally
        //
        // // check if there is a partial line
        // const auto nl = strcspn(_unfinished->raw, return_carriers);
        // if (nl == _unfinished->length) {
        //     if (_unfinished->length >= _unfinished->capacity - Dialect::MINIMAL_CMD_LENGTH) {
        //         DBG("No CRLF found in entire buffer, discarding buffer.")
        //         _unfinished->reset();
        //     }
        //     return {};
        // }
        //
        // // we have a partial line, recover it
        // auto message_buffer = _bufferpool->acquire();
        // if (!message_buffer)
        //     return {};
        // message_buffer->reset();
        //
        // // copy in the discovered line to the message buffer
        // message_buffer->length = nl + 1;
        // assert(_unfinished->length >= message_buffer->length);
        // strlcpy(message_buffer->raw, _unfinished->raw, message_buffer->length);
        //
        // // find all skipable characters
        // const auto skip_length = [return_carriers](const char* str, size_t str_len) {
        //     size_t skip = 0;
        //     while (str_len > 0 && strcspn(str, return_carriers) == 0) {
        //         ++skip;
        //         ++str;
        //         --str_len;
        //     }
        //     return skip;
        // };
        //
        // // move forward all that remains in buffer
        // // todo: ringbuffer ?
        // assert(_unfinished->length >= message_buffer->length);
        // _unfinished->length -= message_buffer->length;
        //
        // const auto skip = skip_length(_unfinished->raw + message_buffer->length, _unfinished->length);
        // assert(_unfinished->length >= skip);
        // _unfinished->length -= skip;
        //
        // memmove(_unfinished->raw, _unfinished->raw + message_buffer->length + skip, _unfinished->length);
        // _unfinished->raw[_unfinished->length] = '\0';
        //
        // // warning, todo: the following debug print needs to be cleared of newline characters, otherwise kaskaspython
        // // does not understand what is line belongs to what
        // // DBG("Returning message: {%s}, remaining(%i) in buffer: {%s}",
        // //      message_buffer->raw,
        // //      _unfinished->length,
        // //      _unfinished->raw);
        // // HAL::delay_ms(200);
        //
        // return Message::from_buffer(std::move(message_buffer));
    }

protected:
    size_t write(const std::string_view& view) { return write((uint8_t*)view.data(), view.length()); }
    size_t write(int8_t* buffer, size_t length) { return write((uint8_t*)buffer, length); };
    virtual size_t write(uint8_t* buffer, size_t length) {
        assert(!"Virtual base function called");
        return -1;
    };
    virtual size_t write(uint8_t& value) {
        assert(!"Virtual base function called");
        return -1;
    };

    size_t read(const int8_t* buffer, size_t length) { return read((uint8_t*)buffer, length); };
    size_t read(const char* buffer, size_t length) { return read((uint8_t*)buffer, length); };
    virtual size_t read(uint8_t* buffer, size_t length) {
        assert(!"Virtual base function called");
        return -1;
    };
    virtual bool read(uint8_t& value) {
        assert(!"Virtual base function called");
        return false;
    };

    virtual size_t available() {
        assert(!"Virtual base function called");
        return -1;
    };

protected:
    Config _cfg;
    std::shared_ptr<Pool<StaticString>> _bufferpool;
    spn::structure::LineBuffer _input_buffer;

    // std::shared_ptr<CharBuffer> _unfinished;
};

class SerialDatalink final : public Datalink {
public:
    SerialDatalink(const Config&& cfg, HAL::UART&& uart) : Datalink(std::move(cfg)), _uart(uart){};
    ~SerialDatalink() override = default;

public:
    void initialize() override { assert(_bufferpool); }

    size_t available() override { return _uart.available(); }

    size_t write(uint8_t& value) override { return _uart.write(value); }
    size_t write(uint8_t* buffer, size_t length) override { return _uart.write(buffer, length); }

    bool read(uint8_t& value) override {
        value = _uart.read();
        return true;
    }
    size_t read(uint8_t* buffer, size_t length) override { return _uart.read(buffer, length); }

private:
    HAL::UART _uart;
};

class MockDatalink final : public Datalink {
public:
    MockDatalink(const Config&& cfg) :
        Datalink(std::move(cfg)),
        _stdin(128), // todo: fully arbitrary number after cleanup of DataLink
        _stdout(128),
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
    std::shared_ptr<Pool<StaticString>>& bufferpool() { return _bufferpool; }

protected:
    using Vector = spn::structure::Vector<std::string>;

    void swap_streams() {
        assert(_active_in != nullptr);
        assert(_active_out != nullptr);
        const auto temp = _active_in;
        _active_in = _active_out;
        _active_out = temp;
    }
    size_t write(uint8_t& value) override {
        assert(!"not implemented!");
        return -1;
    }
    size_t write(uint8_t* buffer, size_t length) override {
        std::string s((char*)buffer, length);
        if (length == 0 || _active_out->full())
            return 0;
        _active_out->push_back(s);
        return length;
    }

    bool read(uint8_t& value) override {
        assert(!"not implemented");
        return -1;
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
