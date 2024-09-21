#pragma once
#include <spine/core/debugging.hpp>
#include <spine/core/utils/concatenate.hpp>

#include <string_view>

namespace kaskas::prompt {

struct Dialect {
    static constexpr std::string_view API_VERSION = "v0.0.1";
    static constexpr std::string_view REPLY_CRLF = "\r\n";

    static constexpr std::string_view OPERANT_REQUEST = ":";
    static constexpr std::string_view OPERANT_REPLY = "<";
    static constexpr std::string_view OPERANT_PRINT_USAGE = "?";

    enum class OP { REQUEST, REPLY, PRINT_USAGE, NOP }; // make sure this matches order of OPERANTS below
    static constexpr std::string_view OPERANTS = ":<?";

    static constexpr std::string_view KV_SEPARATOR = ":";
    static constexpr std::string_view VALUE_SEPARATOR = "|";

    static constexpr OP optype_for_operant(const char operant) {
        for (size_t i = 0; i < OPERANTS.size(); ++i) {
            if (OPERANTS[i] == operant) {
                return static_cast<OP>(i); // Use the index directly to return the corresponding OP
            }
        }
        return OP::NOP; // Default for unrecognized operators
    }
};

} // namespace kaskas::prompt
