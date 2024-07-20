#pragma once
// #include <fmt/core.h>
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

    static constexpr auto REPLY_HEADER = "@";
    static constexpr auto REPLY_FOOTER = ">";
    enum class OP { REQUEST, REPLY, PRINT_USAGE, NOP, SIZE }; // make sure this matches order of OPERANTS

    static constexpr char OPERANT_REQUEST[] = ":";
    static constexpr char OPERANT_REPLY[] = "<";
    static constexpr char OPERANT_PRINT_USAGE[] = "?";
    static constexpr auto OPERANTS = spn::core::utils::concatenate(OPERANT_REQUEST, OPERANT_REPLY, OPERANT_PRINT_USAGE);

    static constexpr auto KV_SEPARATOR = ":";
    static constexpr auto VALUE_SEPARATOR = "|";

    static constexpr auto MINIMAL_CMD_LENGTH = 2;
    static constexpr auto MAXIMAL_CMD_LENGTH = 3;

    // todo: spice this up with some FMTLIB sauce?
    static constexpr auto USAGE_STRING = spn::core::utils::concatenate(
        "KasKas: API version ",
        Dialect::API_VERSION,
        ". Usage:\n\r",
        "  ?                         : print this help\n\r",
        "  MOD:CMD                   : call a command of a module\n\r",
        "  MOD:CMD:ARG|ARG2          : call a command of a module and provide arguments\n\r",
        "\n\r",
        "Replies to API requests look as follows: @CMD:STATUSCODE<ARG1|ARG2\n\r");

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
