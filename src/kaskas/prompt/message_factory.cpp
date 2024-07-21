#include "kaskas/prompt/message_factory.hpp"

#include "kaskas/prompt/message.hpp"

namespace kaskas::prompt {

std::optional<Message> MessageFactory::from_buffer(std::shared_ptr<StaticString> buffer) {
    assert(buffer && buffer->capacity() > 0);
    DBG("from_buffer: incoming {%s}", buffer->c_str());

    if (buffer->starts_with(Dialect::OPERANT_PRINT_USAGE)) {
        DBG("this is a usage request");
        return create_usage_message();
    }

    const auto operant_idx = strcspn(buffer->c_str(), Dialect::OPERANTS.data());
    if (!is_valid_command_size(buffer->size(), operant_idx)) {
        return std::nullopt;
    }

    return parse_buffer(buffer, operant_idx);
}

std::optional<Message> MessageFactory::from_result(std::shared_ptr<StaticString>&& buffer,
                                                   const RPCResult& result,
                                                   const std::string_view& module) {
    assert(buffer);
    assert(buffer->capacity() > 0);

    DBG("from_result: entry: {%s}, module: {%s}, retval: {%s}",
        buffer->c_str(),
        module.data(),
        result.return_value.value().c_str());

    Message m;
    m._buffer = std::move(buffer);
    current_message = &m; // Initialize the current message
    buffer_capacity = current_message->_buffer->capacity();

    head = current_message->_buffer->data();
    head = append_module(module);
    head = append_operant();
    head = append_status(result.status);
    head = append_arguments(result.return_value);

    DBG(" before fun");
    // Check that we have not exceeded the buffer capacity
    if (check_buffer_overflow(head - current_message->_buffer->data())) {
        DBG("Buffer overflow detected");
        return std::nullopt;
    }

    current_message->_buffer->set_size(head - current_message->_buffer->data());
    DBG("fun: {%s}", current_message->_buffer->c_str());

    DBG("from_result: leaving factory: {%s}", current_message->as_string().c_str());

    return std::move(*current_message);
}

bool MessageFactory::is_valid_command_size(size_t buffer_size, size_t operant_idx) {
    return operant_idx != buffer_size - 1 && operant_idx >= Dialect::MINIMAL_CMD_LENGTH
           && operant_idx <= Dialect::MAXIMAL_CMD_LENGTH;
}

std::optional<Message> MessageFactory::create_usage_message() {
    Message m;
    m._module = Dialect::OPERANT_PRINT_USAGE;
    m._operant = Dialect::OPERANT_PRINT_USAGE;
    return m;
}

std::optional<Message> MessageFactory::parse_buffer(std::shared_ptr<StaticString> buffer, size_t operant_idx) {
    Message m;
    current_message = &m; // Initialize the current message
    head = const_cast<char*>(buffer->c_str()); // Initialize head from buffer data
    m._module = std::string_view(head, operant_idx);

    head += operant_idx;
    m._operant = std::string_view(head, 1);

    head += 1;
    const auto kv_idx = strcspn(head, Dialect::KV_SEPARATOR);
    bool newline_at_end = buffer->c_str()[buffer->size() - 1] == '\n';
    if (kv_idx < buffer->size() - (head - buffer->c_str())) {
        m._cmd = std::string_view(head, kv_idx);
        head += kv_idx + 1;
        const auto arguments_length = buffer->size() - (head - buffer->c_str()) - newline_at_end;
        m._arguments =
            arguments_length > 0 ? std::make_optional(std::string_view(head, arguments_length)) : std::nullopt;
    } else {
        m._cmd = std::string_view(head, buffer->size() - (head - buffer->c_str()) - newline_at_end);
        m._arguments = std::nullopt;
    }

    DBG("from_buffer: returning {%s}", m.as_string().c_str());
    m._buffer = std::move(buffer);
    return std::move(m);
}

char* MessageFactory::append_module(const std::string_view& module_value) {
    size_t len = module_value.length();
    if (check_buffer_overflow(len)) {
        DBG("Buffer overflow detected while appending module");
        return head; // No modification made
    }
    strncpy(head, module_value.data(), len);
    current_message->_module = std::string_view(head, len);
    return head + len;
}

char* MessageFactory::append_operant() {
    if (check_buffer_overflow(1)) {
        DBG("Buffer overflow detected while appending operant");
        return head; // No modification made
    }
    strncpy(head, Dialect::OPERANT_REPLY, 1);
    current_message->_operant = std::string_view(head, 1);
    return head + 1;
}

char* MessageFactory::append_status(const RPCResult::Status status) {
    const auto status_str = std::to_string(magic_enum::enum_integer(status));
    size_t len = status_str.length();
    if (check_buffer_overflow(len)) {
        DBG("Buffer overflow detected while appending status");
        return head; // No modification made
    }
    strncpy(head, status_str.c_str(), len);
    current_message->_cmd = std::string_view(head, len);
    return head + len;
}

char* MessageFactory::append_arguments(const std::optional<std::string>& return_value) {
    if (return_value) {
        size_t len = 1 + return_value->length();
        if (check_buffer_overflow(len)) {
            DBG("Buffer overflow detected while appending arguments");
            return head; // No modification made
        }
        strncpy(head, ":", 1);
        head += 1;
        strncpy(head, return_value->c_str(), return_value->length());
        current_message->_arguments = std::string_view(head, return_value->length());
        return head + return_value->length();
    }
    return head;
}

bool MessageFactory::check_buffer_overflow(size_t data_length) {
    DBG("overflowcheck: datalength: %i, capacity left: %i",
        data_length,
        buffer_capacity - (head - current_message->_buffer->data()));
    return data_length > buffer_capacity - (head - current_message->_buffer->data());
}

} // namespace kaskas::prompt
