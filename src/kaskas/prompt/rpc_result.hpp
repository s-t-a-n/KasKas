#pragma once

#include <magic_enum/magic_enum.hpp>
#include <spine/core/debugging.hpp>
#include <spine/platform/hal.hpp>

#include <optional>
#include <string_view>

namespace kaskas::prompt {

using OptString = std::optional<std::string>;
using OptStringView = std::optional<std::string>;
// using OptStringView = std::optional<std::string_view>;
using OptInt = std::optional<int>;

struct RPCResult {
    RPCResult(const RPCResult& other) : return_value(other.return_value), status(other.status) {}
    RPCResult(RPCResult&& other) noexcept : return_value(other.return_value), status(std::move(other.status)) {}
    RPCResult& operator=(const RPCResult& other) {
        if (this == &other)
            return *this;
        return_value = other.return_value;
        status = other.status;
        return *this;
    }
    RPCResult& operator=(RPCResult&& other) noexcept {
        if (this == &other)
            return *this;
        return_value = other.return_value;
        status = std::move(other.status);
        return *this;
    }
    enum class Status { UNDEFINED, OK, BAD_INPUT, BAD_RESULT };

    RPCResult(OptStringView&& return_value) : return_value(std::move(return_value)), status(Status::OK) {}
    RPCResult(OptStringView&& return_value, Status status) : return_value(std::move(return_value)), status(status) {}
    RPCResult(Status status) : return_value(std::string(magic_enum::enum_name(status))), status(status) {}

    OptString return_value;
    Status status;
};

} // namespace kaskas::prompt