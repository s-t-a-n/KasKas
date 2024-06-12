#pragma once
#include <spine/core/debugging.hpp>

#include <AH/STL/memory>
#include <cstring>

namespace kaskas::prompt {

class CharBuffer {
private:
    std::unique_ptr<char> _buffer;

public:
    char* raw;
    const size_t capacity;
    size_t length = 0;

    void reset(bool zero = true) {
        length = 0;
        memset((unsigned char*)raw, '\0', capacity);
    }

    explicit CharBuffer(size_t buffer_length)
        : _buffer(std::make_unique<char>(buffer_length)), raw(_buffer.get()), capacity(buffer_length) {}

    ~CharBuffer() {
        DBGF("Charbuffer scuddling off our mortal coil");
        delete _buffer.get();
        _buffer.reset();
    }
};

} // namespace kaskas::prompt