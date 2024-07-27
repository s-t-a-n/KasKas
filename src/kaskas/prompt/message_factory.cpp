// #include "kaskas/prompt/message_factory.hpp"
//
// #include "kaskas/prompt/message.hpp"
//
// #include <spine/core/utils/string.hpp>
//
// namespace kaskas::prompt {
//
// // std::optional<Message> IncomingMessageFactory::from_view(const std::string_view& view) {
// //     DBG("from_buffer: incoming {%s}", std::string(view).c_str());
// //
// //     if (spn::core::starts_with(view, Dialect::OPERANT_PRINT_USAGE)) {
// //         DBG("this is a usage request");
// //         return create_usage_message();
// //     }
// //
// //     // const auto operant_idx = strcspn(view.c_str(), Dialect::OPERANTS.data());
// //     const auto operant_idx = spn::core::find_first_of(view, Dialect::OPERANTS);
// //     if (!is_valid_command_sizeis_valid_command_size(view.size(), operant_idx)) {
// //         return std::nullopt;
// //     }
// //
// //     return parse_view(view, operant_idx);
// // }
//
// std::optional<MessageWithStorage<RPCResult>> IncomingMessageFactory::from_result(const RPCResult& result,
//                                                                                  const std::string_view& module) {
//     DBG("from_result: entry: {%s}, module: {%s}, retval: {%s}",
//         buffer->c_str(),
//         module.data(),
//         result.return_value.value().c_str());
//
//     Message m;
//     m._buffer = std::move(buffer);
//     current_message = &m; // Initialize the current message
//     buffer_capacity = current_message->_buffer->capacity();
//
//     head = current_message->_buffer->data();
//     head = append_module(module);
//     head = append_operant();
//     head = append_status(result.status);
//     head = append_arguments(result.return_value);
//
//     DBG(" before fun");
//     // Check that we have not exceeded the buffer capacity
//     if (check_buffer_overflow(head - current_message->_buffer->data())) {
//         DBG("Buffer overflow detected");
//         return std::nullopt;
//     }
//
//     current_message->_buffer->set_size(head - current_message->_buffer->data());
//     DBG("fun: {%s}", current_message->_buffer->c_str());
//
//     DBG("from_result: leaving factory: {%s}", current_message->as_string().c_str());
//
//     return std::move(*current_message);
// }
//
// std::optional<Message> IncomingMessageFactory::create_usage_message() {
//     Message m;
//     m._module = Dialect::OPERANT_PRINT_USAGE;
//     m._operant = Dialect::OPERANT_PRINT_USAGE;
//     return m;
// }
//
// // std::optional<Message> IncomingMessageFactory::parse_view(const std::string_view& view, size_t operant_idx) {
// //     Message m;
// //     current_message = &m; // Initialize the current message
// //     head = const_cast<char*>(view.data()); // Initialize head from buffer data
// //     m._module = std::string_view(head, operant_idx);
// //
// //     head += operant_idx;
// //     m._operant = std::string_view(head, 1);
// //
// //     head += 1;
// //
// //     DBG("from_buffer: returning {%s}", m.as_string().c_str());
// //     m._buffer = std::move(buffer);
// //     return std::move(m);
// // }
//
// char* IncomingMessageFactory::append_module(const std::string_view& module_value) {
//     size_t len = module_value.length();
//     if (check_buffer_overflow(len)) {
//         DBG("Buffer overflow detected while appending module");
//         return head; // No modification made
//     }
//     strncpy(head, module_value.data(), len);
//     current_message->_module = std::string_view(head, len);
//     return head + len;
// }
//
// char* IncomingMessageFactory::append_operant() {
//     if (check_buffer_overflow(1)) {
//         DBG("Buffer overflow detected while appending operant");
//         return head; // No modification made
//     }
//     strncpy(head, Dialect::OPERANT_REPLY, 1);
//     current_message->_operant = std::string_view(head, 1);
//     return head + 1;
// }
//
// char* IncomingMessageFactory::append_status(const RPCResult::Status status) {
//     const auto status_str = std::to_string(magic_enum::enum_integer(status));
//     size_t len = status_str.length();
//     if (check_buffer_overflow(len)) {
//         DBG("Buffer overflow detected while appending status");
//         return head; // No modification made
//     }
//     strncpy(head, status_str.c_str(), len);
//     current_message->_cmd = std::string_view(head, len);
//     return head + len;
// }
//
// char* IncomingMessageFactory::append_arguments(const std::optional<std::string>& return_value) {
//     if (return_value) {
//         size_t len = 1 + return_value->length();
//         if (check_buffer_overflow(len)) {
//             DBG("Buffer overflow detected while appending arguments");
//             return head; // No modification made
//         }
//         strncpy(head, ":", 1);
//         head += 1;
//         strncpy(head, return_value->c_str(), return_value->length());
//         current_message->_arguments = std::string_view(head, return_value->length());
//         return head + return_value->length();
//     }
//     return head;
// }
//
// bool IncomingMessageFactory::check_buffer_overflow(size_t data_length) {
//     DBG("overflowcheck: datalength: %i, capacity left: %i",
//         data_length,
//         buffer_capacity - (head - current_message->_buffer->data()));
//     return data_length > buffer_capacity - (head - current_message->_buffer->data());
// }
//
// } // namespace kaskas::prompt
