#pragma once

#include "kaskas/prompt/StaticString.hpp"
#include "kaskas/prompt/dialect.hpp"
#include "kaskas/prompt/rpc_result.hpp"

#include <cstring>
#include <optional>
#include <string_view>

namespace kaskas::prompt {

class Message; // Forward declaration of Message

class MessageFactory {
public:
    /// Creates a Message from the provided buffer.
    std::optional<Message> from_buffer(std::shared_ptr<StaticString> buffer);

    /// Creates a Message from an RPC result and a buffer.
    std::optional<Message>
    from_result(std::shared_ptr<StaticString>&& buffer, const RPCResult& result, const std::string_view& module);

private:
    /// Validates if the command size is within acceptable limits.
    static bool is_valid_command_size(size_t buffer_size, size_t operant_idx);

    /// Creates a message indicating usage instructions.
    static std::optional<Message> create_usage_message();

    /// Parses the buffer and extracts the message components.
    std::optional<Message> parse_buffer(std::shared_ptr<StaticString> buffer, size_t operant_idx);

    /// Appends the module to the buffer and returns the updated head pointer.
    char* append_module(const std::string_view& module_value);

    /// Appends the operant to the buffer and returns the updated head pointer.
    char* append_operant();

    /// Appends the status to the buffer and returns the updated head pointer.
    char* append_status(const RPCResult::Status status);

    /// Appends the arguments to the buffer and returns the updated head pointer.
    char* append_arguments(const std::optional<std::string>& return_value);

    /// Checks for buffer overflow before writing data.
    bool check_buffer_overflow(size_t data_length);

    char* head; ///< Pointer used for buffer manipulation.
    size_t buffer_capacity; ///< Capacity of the buffer to ensure we do not exceed it.
    Message* current_message; ///< Pointer to the current message being processed.
};

} // namespace kaskas::prompt
