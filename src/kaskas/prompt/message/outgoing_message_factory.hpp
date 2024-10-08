#pragma once

#include "kaskas/prompt/dialect.hpp"
#include "kaskas/prompt/rpc/result.hpp"
#include "message.hpp"

#include <spine/core/utils/string.hpp>
#include <spine/structure/result.hpp>

#include <cstring>
#include <optional>
#include <string_view>

namespace kaskas::prompt {

class Message; // Forward declaration of Message

class OutgoingMessageFactory {
public:
    enum class Error : uint8_t {};

    /// Creates a Message from an RPC result and a buffer.
    static spn::structure::Result<MessageWithStorage<RPCResult>, Error>
    from_rpc_result(RPCResult&& result, const std::string_view& module) {
        const auto parse = [](ParseContext&& context) -> ParseResult {
            return ParseResult::intermediary(std::move(context))
                .chain([](ParseContext& ctx) { return parse_operant(ctx); })
                .chain([](ParseContext& ctx) { return parse_status(ctx); })
                .chain([](ParseContext& ctx) { return parse_arguments(ctx); })
                .chain([](ParseContext& ctx) { return parse_finalizer(ctx); });
        };

        auto persistent_result = std::make_unique<RPCResult>(result);
        auto parse_result = parse(ParseContext(*persistent_result, module));

        if (parse_result.is_success()) {
            return MessageWithStorage<RPCResult>(std::move(parse_result.unwrap()), std::move(persistent_result));
        }

        if (parse_result.is_failed()) {
            return spn::structure::Result<MessageWithStorage<RPCResult>, Error>::failed(
                parse_result.unwrap_error_value());
        }

        return {};
    }

private:
    struct ParseContext {
        explicit ParseContext(const RPCResult& result, const std::string_view& module)
            : result(result), module(module) {}
        const RPCResult& result;
        std::string_view module;
        std::string_view operant;
        std::string_view status;
        std::optional<std::string_view> arguments;
    };

    using ParseResult = spn::structure::Result<Message, Error, ParseContext>;

    static ParseResult parse_operant(ParseContext& ctx) {
        ctx.operant = Dialect::OPERANT_REPLY;
        return ParseResult::intermediary(ctx);
    }
    static ParseResult parse_status(ParseContext& ctx) {
        ctx.status = kaskas::prompt::detail::numeric_status(ctx.result.status);
        return ParseResult::intermediary(std::move(ctx));
    }
    static ParseResult parse_arguments(ParseContext& ctx) {
        if (auto& args = ctx.result.return_value; args) ctx.arguments = args.value();
        return ParseResult::intermediary(std::move(ctx));
    }
    static ParseResult parse_finalizer(ParseContext& ctx) {
        return ParseResult(Message(ctx.module, ctx.operant, ctx.status, ctx.arguments));
    }
};

} // namespace kaskas::prompt
