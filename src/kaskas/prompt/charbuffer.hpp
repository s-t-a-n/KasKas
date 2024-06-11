#pragma once
#include <AH/STL/memory>
#include <cstring>

namespace kaskas::prompt {

class CharBuffer {
private:
    const std::unique_ptr<char> _buffer;

public:
    char* raw;
    const size_t capacity;
    size_t length = 0;

    void reset(bool zero = true) {
        length = 0;
        memset(raw, '\0', capacity);
    }

    explicit CharBuffer(size_t buffer_length)
        : _buffer(std::unique_ptr<char>(new char[buffer_length])), raw(_buffer.get()), capacity(buffer_length) {}
};

} // namespace kaskas::prompt