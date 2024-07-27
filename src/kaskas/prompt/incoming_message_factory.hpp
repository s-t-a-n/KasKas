#pragma once

#include "kaskas/prompt/dialect.hpp"
#include "kaskas/prompt/rpc_result.hpp"
#include "message.hpp"

#include <spine/core/utils/string.hpp>
#include <spine/structure/result.hpp>

#include <cstring>
#include <optional>
#include <string_view>

namespace kaskas::prompt {

class Message; // Forward declaration of Message

class IncomingMessageFactory {
public:
    /// Create a Message from the provided string_view.
    static std::optional<Message> from_view(const std::string_view& view) {
        const auto parse = [](ParseContext& context) -> ParseResult {
            return ParseResult::intermediary(context)
                .chain([](ParseContext& ctx) { return throw_out_empty_messages(ctx); })
                .chain([](ParseContext& ctx) { return parse_usage_request(ctx); })
                .chain([](ParseContext& ctx) { return parse_operant(ctx); })
                .chain([](ParseContext& ctx) { return parse_command(ctx); })
                .chain([](ParseContext& ctx) { return parse_arguments(ctx); })
                .chain([](ParseContext& ctx) { return parse_finalizer(ctx); });
        };

        auto ctx = ParseContext(view);
        auto result = parse(ctx);

        if (result.is_success()) {
            return result.unwrap();
        }

        if (result.is_failed()) {
            // DBG("Failed extracting message from input: %s", std::string(view).c_str());
        }

        return {};
    }

private:
    struct ParseContext {
        explicit ParseContext(const std::string_view& view) : view(view), head(view.data()) {}
        std::string_view view;
        const char* head;

        std::string_view module;
        std::string_view operant;
        std::optional<std::string_view> status;
        std::optional<std::string_view> arguments;
    };

    using ParseResult = spn::structure::Result<Message, std::string_view, ParseContext>;

    static ParseResult throw_out_empty_messages(ParseContext& ctx) {
        if (ctx.view.size() == 0) {
            return ParseResult::failed("Empty message");
        }
        return ParseResult::intermediary(ctx);
    }

    static ParseResult parse_usage_request(ParseContext& ctx) {
        if (spn::core::starts_with(ctx.view, Dialect::OPERANT_PRINT_USAGE)) {
            DBG("usage request detected");
            return Message(Dialect::OPERANT_PRINT_USAGE, Dialect::OPERANT_PRINT_USAGE);
        }
        return ParseResult::intermediary(ctx);
    }

    static ParseResult parse_operant(ParseContext& ctx) {
        assert(ctx.head);

        const auto validate_command_size = [](size_t op) {
            return op >= Dialect::MINIMAL_CMD_LENGTH && op <= Dialect::MAXIMAL_CMD_LENGTH;
        };

        if (auto op = spn::core::find_first_of(ctx.head, Dialect::OPERANTS_V); op != std::string::npos) {
            if (!validate_command_size(op)) {
                return ParseResult::failed("Illegal command size");
            }
            ctx.module = std::string_view(ctx.head, op);
            ctx.head += op;
            ctx.operant = std::string_view(ctx.head, 1);
            ctx.head += 1;
            return ParseResult::intermediary(ctx);
        }
        return ParseResult::failed("Couldn't find operant");
    }

    static ParseResult parse_command(ParseContext& ctx) {
        if (auto delim = spn::core::find_first_of(ctx.head, Dialect::KV_SEPARATOR); delim != std::string::npos) {
            ctx.status = std::string_view(ctx.head, delim);
            ctx.head += delim + 1; // +1 to remove the delim itself as well
            return ParseResult::intermediary(ctx);
        }
        const auto remaining = ctx.view.size() - (ctx.head - ctx.view.data());
        ctx.status = std::string_view(ctx.head, remaining);
        return ParseResult(Message(ctx.module, ctx.operant, ctx.status));
    }

    static ParseResult parse_arguments(ParseContext& ctx) {
        const auto remaining = ctx.view.size() - (ctx.head - ctx.view.data());
        ctx.arguments = std::string_view(ctx.head, remaining);
        return ParseResult::intermediary(ctx);
    }

    static ParseResult parse_finalizer(ParseContext& ctx) {
        return ParseResult(Message(ctx.module, ctx.operant, ctx.status, ctx.arguments));
    }
};

} // namespace kaskas::prompt
