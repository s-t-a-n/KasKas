#pragma once
#include <magic_enum/magic_enum.hpp>
#include <magic_enum/magic_enum_all.hpp>
#include <spine/core/debugging.hpp>
#include <spine/core/utils/concatenate.hpp>

#include <array>
#include <cstring>
#include <numeric>
#include <utility>

namespace kaskas::prompt {

struct Dialect {
    static constexpr char API_VERSION[] = "0.0.0";

    static constexpr std::string_view REPLY_HEADER = "@";
    static constexpr std::string_view REPLY_FOOTER = ">";
    enum class OP { REQUEST, REPLY, PRINT_USAGE, NOP, SIZE }; // make sure this matches order of OPERANTS

    static constexpr char OPERANT_REQUEST[] = ":";
    static constexpr char OPERANT_REPLY[] = "<";
    static constexpr char OPERANT_PRINT_USAGE[] = "?";

    // static constexpr std::string_view OPERANTS = ":<?";

    // the following seems up to impossible in C++17; every solution under the sun I have tried,
    // we can only wonder why C++ is considered a difficult language. Just to tie together three chars.
    static constexpr auto OPERANTS = spn::core::utils::concat(OPERANT_REQUEST, OPERANT_REPLY, OPERANT_PRINT_USAGE);
    static constexpr std::string_view OPERANTS_V = {OPERANTS.data(), OPERANTS.size()};

    static constexpr std::string_view KV_SEPARATOR = ":";
    static constexpr std::string_view VALUE_SEPARATOR = "|";

    static constexpr auto MINIMAL_CMD_LENGTH = 1;
    static constexpr auto MAXIMAL_CMD_LENGTH = 10;

    // todo: spice this up with some FMTLIB sauce?
    static constexpr auto USAGE_STRING = spn::core::utils::concat("KasKas: API version ",
                                                                  Dialect::API_VERSION,
                                                                  ". Usage:\n\r",
                                                                  "\n\r",
                                                                  "Requests to the API must look as follows: \n\r",
                                                                  "\t\tMODULE:CMD:ARG1|ARG2\n\r"
                                                                  "Replies to API requests look as follows:\n\r",
                                                                  "\t\t@CMD:STATUSCODE<ARG1|ARG2\n\r",
                                                                  "\n\rModules:\n\r");
    static constexpr std::string_view USAGE_STRING_V = {USAGE_STRING.data(), USAGE_STRING.size()};

    static OP optype_for_operant(const char operant) {
        auto found_op = OP::NOP;
        // todo : use std::find instead, it gave a funny icw magic_enum
        magic_enum::enum_for_each<OP>([operant, &found_op](OP optype) {
            const auto op_idx = *magic_enum::enum_index(optype);
            if (op_idx < std::strlen(OPERANTS.data()) && operant == OPERANTS[op_idx]) {
                found_op = optype;
            }
        });
        return found_op;
    }
};

} // namespace kaskas::prompt
