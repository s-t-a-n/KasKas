#pramga once
#include <spine/core/debugging.hpp>
#include <spine/structure/pool.hpp>

#include <AH/STL/memory>
#include <optional>

namespace kaskas::prompt {
struct ByteBuffer {
    std::unique_ptr<uint8_t> buffer;
    size_t buffer_length;

    explicit ByteBuffer(size_t buffer_length)
        : buffer(std::unique_ptr<uint8_t>(new uint8_t[buffer_length])), buffer_length(buffer_length) {}
};

class Message {
public:
    ByteBuffer& buffer() {
        assert(_buffer);
        return *_buffer;
    }

private:
    std::shared_ptr<ByteBuffer> _buffer;

    size_t _cmd;
    size_t _operant;
    size_t _argument;
};

class Datalink {
public:
    struct Config {
        size_t pool_size;
    };

    Datalink(const Config&& cfg) : _cfg(cfg), _mpool(_cfg.pool_size) {}

public:
    virtual ~Datalink() = default;
    virtual void send_message(const Message& msg) = 0;
    virtual std::optional<Message> receive_message() = 0;

private:
    Config _cfg;
    spn::structure::Pool<Message> _mpool;
};

class SerialDatalink : public Datalink {
    using Datalink::Datalink;

public:
    void send_message(const Message& msg) override {}
    std::optional<Message> receive_message() override { return std::nullopt; }

private:
};

class MiddleWare {};

class Prompt {};

} // namespace kaskas::prompt