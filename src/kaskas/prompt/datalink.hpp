#pragma once

#include "kaskas/prompt/charbuffer.hpp"
#include "kaskas/prompt/message.hpp"

#include <spine/core/assert.hpp>
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

    // todo migrate this to a proper Stream like class, so as to avoid the very smelly duplication of code below
    virtual ~Datalink() = default;
    virtual void initialize() = 0;
    void send_message(const Message& msg) {
        write(msg.cmd());
        write(msg.operant());
        if (msg.key()) {
            write(msg.key()->data());
        }
        if (msg.value()) {
            write(std::string_view(":"));
            write(msg.value()->data());
        }
        write(std::string_view("\n"));
    }

    std::optional<Message> receive_message() {
        if (available() == 0)
            return {};
        if (!_unfinished) {
            _unfinished = std::move(_bufferpool->acquire());
            _unfinished.reset();
        }
        _unfinished->length += read(_unfinished->raw, _unfinished->capacity - _unfinished->length);
        if (_unfinished->length == 0)
            return {};

        if (_unfinished->raw[_unfinished->length - 1] == '\n') {
            // what a stroke of luck, an entire line!
            return Message::from_buffer(std::move(_unfinished));
        }

        // check if there is  a partial line
        const auto nl = strcspn(_unfinished->raw, "\n");
        if (nl == _unfinished->length)
            return {};

        // we have a partial line, recover it
        auto newbuffer = _bufferpool->acquire();
        if (!newbuffer)
            return {};
        newbuffer->reset();

        // copy in the line
        strlcpy(newbuffer->raw, _unfinished->raw, nl);
        newbuffer->length = nl;

        // move forward the remaining of the unfinished line
        memmove(_unfinished->raw, _unfinished->raw + nl, _unfinished->length - nl);
        _unfinished->length = _unfinished->length - nl;
        _unfinished->raw[_unfinished->length] = '\0';

        return Message::from_buffer(std::move(newbuffer));
    }

protected:
    size_t write(const std::string_view& view) { write((uint8_t*)view.data(), view.length()); }
    size_t write(int8_t* buffer, size_t length) { return write((uint8_t*)buffer, length); };
    virtual size_t write(uint8_t* buffer, size_t length) = 0;

    size_t read(const int8_t* buffer, size_t length) { return read((uint8_t*)buffer, length); };
    size_t read(const char* buffer, size_t length) { return read((uint8_t*)buffer, length); };
    virtual size_t read(uint8_t* buffer, size_t length) = 0;

    virtual size_t available() = 0;

protected:
    Config _cfg;
    std::shared_ptr<Pool<CharBuffer>> _bufferpool;

    std::shared_ptr<CharBuffer> _unfinished;
};

class SerialDatalink : public Datalink {
public:
    SerialDatalink(const Config&& cfg, HAL::UART&& uart) : Datalink(std::move(cfg)), _uart(uart){};
    ~SerialDatalink() override = default;

public:
    void initialize() override { assert(_bufferpool); }

    void send_message(const Message& msg) override {
        _uart.write(reinterpret_cast<const uint8_t*>(msg.cmd().data()), msg.cmd().length());
        _uart.write(reinterpret_cast<const uint8_t*>(msg.operant().data()), msg.operant().length());
        if (msg.key()) {
            _uart.write(reinterpret_cast<const uint8_t*>(msg.key()->data()), msg.key()->length());
        }
        if (msg.value()) {
            _uart.write(reinterpret_cast<const uint8_t*>(":"), 1);
            _uart.write(reinterpret_cast<const uint8_t*>(msg.value()->data()), msg.value()->length());
        }
        _uart.write('\n');
    }
    std::optional<Message> receive_message() override {
        if (_uart.available() == 0)
            return std::nullopt;
        assert(_bufferpool);
        auto buffer = _bufferpool->acquire();
        if (!buffer)
            return std::nullopt;
        buffer->length = _uart.read(reinterpret_cast<uint8_t*>(buffer->raw), buffer->capacity);
        buffer->raw[buffer->length < buffer->capacity ? buffer->length : buffer->capacity - 1] = '\0';
        return Message::from_buffer(std::move(buffer));
    }

private:
    HAL::UART& _uart;
};

class MockDatalink : public Datalink {
public:
    MockDatalink(const Config&& cfg) : Datalink(std::move(cfg)), _stdin(_cfg.pool_size), _stdout(_cfg.pool_size){};
    ~MockDatalink() override = default;

public:
    void initialize() override { assert(_bufferpool); }

    void send_message(const Message& msg) override { send_message_impl(msg, _stdout); }
    std::optional<Message> receive_message() override { return receive_message_impl(_stdin); }

    void inject_message(const Message& msg) { send_message_impl(msg, _stdin); }
    std::optional<Message> extract_reply() { return receive_message_impl(_stdout); }

    std::shared_ptr<Pool<CharBuffer>>& bufferpool() { return _bufferpool; }

protected:
    using Vector = spn::structure::Vector<std::string>;

    void send_message_impl(const Message& msg, Vector& out) {
        std::string s =
            std::string(msg.cmd()) + std::string(msg.operant()) + std::string(msg.value()) + std::string("\n");
        if (msg.key()) {
        }

        if (msg.value()) {
        }

        out.push_back(s);
        DBGF("send_message_impl: pushed back: %s", out.back().c_str());
    }

    std::optional<Message> receive_message_impl(Vector& in) {
        if (_stdin.empty())
            return std::nullopt;
        assert(_bufferpool);
        auto buffer = _bufferpool->acquire();
        if (!buffer)
            return std::nullopt;
        const auto s = _stdin.pop_front();
        std::memcpy(buffer->raw, s.c_str(), s.length());
        buffer->length = s.length();
        buffer->raw[buffer->length < buffer->capacity ? buffer->length : buffer->capacity - 1] = '\0';
        DBGF("receive_message_impl: took from front: %s", static_cast<const char*>(buffer->raw));

        const auto msg = Message::from_buffer(std::move(buffer));

        const auto s1 = std::string(msg->cmd());
        const auto s2 = std::string(msg->operant());
        const auto s3 = std::string(msg->value());
        DBGF("receive_message_impl Message buffer: %s:%s:%s", s1.c_str(), s2.c_str(), s3.c_str());
        return msg;
    }

private:
    Vector _stdin;
    Vector _stdout;
};

} // namespace kaskas::prompt