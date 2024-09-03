#pragma once

#include <magic_enum/magic_enum.hpp>
#include <spine/core/debugging.hpp>
#include <spine/platform/hal.hpp>

#include <optional>
#include <string_view>

namespace kaskas::prompt {

using OptString = std::optional<std::string>;
using OptStringView = std::optional<std::string_view>;
using OptInt = std::optional<int>;

struct RPCResult {
    RPCResult(const RPCResult& other) : return_value(other.return_value), status(other.status) {}
    RPCResult(RPCResult&& other) noexcept : return_value(other.return_value), status(std::move(other.status)) {}
    RPCResult& operator=(const RPCResult& other) {
        if (this == &other) return *this;
        return_value = other.return_value;
        status = other.status;
        return *this;
    }
    RPCResult& operator=(RPCResult&& other) noexcept {
        if (this == &other) return *this;
        return_value = other.return_value;
        status = std::move(other.status);
        return *this;
    }
    enum class Status { UNDEFINED, OK, BAD_INPUT, BAD_RESULT };

    // constexpr std::array<std::string, 5> StatusIntStrs = {magic_enum::enum_integer(RPCResult::Status::BAD_INPUT)};

    explicit RPCResult(OptString&& return_value) : return_value(std::move(return_value)), status(Status::OK) {}
    explicit RPCResult(OptString&& return_value, Status status)
        : return_value(std::move(return_value)), status(status) {}

    RPCResult(Status status) : return_value(std::string(magic_enum::enum_name(status))), status(status) {}

    OptString return_value;
    Status status;
};

namespace detail {
constexpr auto numeric_status_names = []() {
    constexpr auto names = magic_enum::enum_names<RPCResult::Status>();
    std::array<std::string_view, names.size()> result{};
    for (size_t i = 0; i < names.size(); ++i) {
        result[i] = names[i];
    }
    return result;
}();

constexpr const std::string_view& numeric_status(RPCResult::Status status) {
    auto index = magic_enum::enum_index(status);
    if (index.has_value()) {
        return numeric_status_names[index.value()];
    }
    return numeric_status_names[0]; // index 0 -> UNDEFINED
}
} // namespace detail

} // namespace kaskas::prompt