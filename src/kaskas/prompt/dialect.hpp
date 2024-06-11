#pragma once
#include <magic_enum/magic_enum.hpp>
#include <magic_enum/magic_enum_all.hpp>
#include <spine/core/assert.hpp>
#include <spine/core/debugging.hpp>

#include <cstring>

namespace kaskas::prompt {

// ripped from: https://stackoverflow.com/a/65440575/18775667
// we cannot return a char array from a function, therefore we need a wrapper
template<unsigned N>
struct CatString {
    char c[N];
};

template<unsigned... Len>
constexpr auto cat(const char (&... strings)[Len]) {
    constexpr unsigned N = (... + Len) - sizeof...(Len);
    CatString<N + 1> result = {};
    result.c[N] = '\0';

    char* dst = result.c;
    for (const char* src : {strings...}) {
        for (; *src != '\0'; src++, dst++) {
            *dst = *src;
        }
    }
    return result.c;
}

struct Dialect {
    enum class OP { FUNCTION_CALL, ASSIGNMENT, ACCESS, RETURN_VALUE, NOP, SIZE };
    static OP optype_for_operant(const char operant) {
        auto found_op = OP::NOP;
        // todo : use find
        magic_enum::enum_for_each<OP>([operant, &found_op](OP optype) {
            const auto op_idx = *magic_enum::enum_index(optype);
            if (op_idx < std::strlen(OPERANTS) && operant == OPERANTS[op_idx]) {
                DBGF("found op");
                found_op = optype;
            }
        });
        return found_op;
    }

    // todo: constexpr from OPERANTS
    // static constexpr char OPERANT_FUNCTION_CALL[] = "!";
    // static constexpr char OPERANT_ASSIGNMENT[] = "=";
    // static constexpr char OPERANT_ACCESS[] = "?";
    // static constexpr char OPERANT_REPLY[] = "<";
    // static constexpr auto OPERANTS = cat(OPERANT_FUNCTION_CALL, OPERANT_ASSIGNMENT, OPERANT_ACCESS, OPERANT_REPLY);

    static constexpr auto OPERANT_FUNCTION_CALL = "!";
    static constexpr auto OPERANT_ASSIGNMENT = "=";
    static constexpr auto OPERANT_ACCESS = "?";
    static constexpr auto OPERANT_REPLY = "<";
    static constexpr auto OPERANTS = "!=?<";

    static constexpr auto MINIMAL_CMD_LENGTH = 2;
    static constexpr auto MAXIMAL_CMD_LENGTH = 3;
};

} // namespace kaskas::prompt