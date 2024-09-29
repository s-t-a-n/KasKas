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

class IncomingMessageFactory {
public:
    enum class Error : uint8_t { EMPTY, MALFORMED_MODULE, MALFORMED_COMMAND, MALFORMED_OPERANT };

    /// Create a Message from the provided string_view.
    static spn::structure::Result<Message, Error> from_view(const std::string_view& view) {
        if (view.empty()) return spn::structure::Result<Message, Error>::failed(Error::EMPTY);

        const auto parse = [](ParseContext& context) -> ParseResult {
            return ParseResult::intermediary(context)
                .chain([](ParseContext& ctx) { return throw_out_empty_messages(ctx); })
                .chain([](ParseContext& ctx) { return parse_usage_request(ctx); })
                .chain([](ParseContext& ctx) { return throw_out_malformed_messages(ctx); })
                .chain([](ParseContext& ctx) { return parse_operant(ctx); })
                .chain([](ParseContext& ctx) { return parse_command(ctx); })
                .chain([](ParseContext& ctx) { return parse_arguments(ctx); })
                .chain([](ParseContext& ctx) { return parse_finalizer(ctx); });
        };

        auto ctx = ParseContext(view);
        auto result = parse(ctx);

        if (result.is_success()) return result.unwrap();
        if (result.is_failed()) return spn::structure::Result<Message, Error>::failed(result.unwrap_error_value());
        return {};
    }

private:
    struct ParseContext {
        explicit ParseContext(const std::string_view& view) : view(view), head(view.data()) {}
        std::string_view view;

        const char* head;
        size_t remaining() const { return view.size() - (head - view.data()); }

        std::string_view module;
        std::string_view operant;
        std::string_view command;
        std::optional<std::string_view> arguments;
    };

    using ParseResult = spn::structure::Result<Message, Error, ParseContext>;

    static ParseResult throw_out_empty_messages(ParseContext& ctx) {
        // if an incoming string is empty, do not parse any further
        if (ctx.view.size() == 0) return ParseResult::failed(Error::EMPTY);
        return ParseResult::intermediary(ctx);
    }

    static ParseResult parse_usage_request(ParseContext& ctx) {
        // if an incoming string starts with a Dialect::OPERANT_PRINT_USAGE operant, do not parse any further
        if (spn::core::utils::starts_with(ctx.view, Dialect::OPERANT_PRINT_USAGE))
            return Message(Dialect::OPERANT_PRINT_USAGE, Dialect::OPERANT_PRINT_USAGE);
        return ParseResult::intermediary(ctx);
    }

    static ParseResult throw_out_malformed_messages(ParseContext& ctx) {
        // if an incoming string is empty, do not parse any further
        if (ctx.view.size() == 0) return ParseResult::failed(Error::EMPTY);
        return ParseResult::intermediary(ctx);
    }

    static ParseResult parse_operant(ParseContext& ctx) {
        assert(ctx.head);

        if (auto op = spn::core::utils::find_first_of({ctx.head, ctx.remaining()}, Dialect::OPERANT_REQUEST);
            op != std::string::npos) {
            if (op == 0) return ParseResult::failed(Error::MALFORMED_MODULE); // illegal: empty module
            ctx.module = std::string_view(ctx.head, op);
            ctx.head += op;
            ctx.operant = std::string_view(ctx.head, 1);
            ctx.head += 1;
            return ParseResult::intermediary(ctx);
        }
        return ParseResult::failed(Error::MALFORMED_OPERANT);
    }

    static ParseResult parse_command(ParseContext& ctx) {
        if (auto delim = spn::core::utils::find_first_of({ctx.head, ctx.remaining()}, Dialect::KV_SEPARATOR);
            delim != std::string::npos) {
            if (delim == 0) return ParseResult::failed(Error::MALFORMED_COMMAND); // illegal: empty command
            ctx.command = std::string_view(ctx.head, delim);
            ctx.head += delim + 1; // +1 to remove the delim itself as well
            return ParseResult::intermediary(ctx);
        }
        if (ctx.remaining() == 0) return ParseResult::failed(Error::MALFORMED_COMMAND); // illegal: empty command
        ctx.command = std::string_view(ctx.head, ctx.remaining());
        return ParseResult(Message(ctx.module, ctx.operant, ctx.command));
    }

    static ParseResult parse_arguments(ParseContext& ctx) {
        ctx.arguments = std::string_view(ctx.head, ctx.remaining());
        return ParseResult::intermediary(ctx);
    }

    static ParseResult parse_finalizer(ParseContext& ctx) {
        return ParseResult(Message(ctx.module, ctx.operant, ctx.command, ctx.arguments));
    }
};

} // namespace kaskas::prompt
