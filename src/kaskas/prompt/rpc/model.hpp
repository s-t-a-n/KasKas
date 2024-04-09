#pragma once

#include "kaskas/prompt/dialect.hpp"
#include "kaskas/prompt/message/message.hpp"
#include "kaskas/prompt/rpc/result.hpp"

#include <spine/core/debugging.hpp>
#include <spine/platform/hal.hpp>
#include <spine/structure/result.hpp>

namespace kaskas::prompt {

/// A model of a remote executable procedure call.
class RPCModel {
public:
    // RPCModel(const std::string& name) : _name(name) {}
    RPCModel(const std::string& name, const std::function<RPCResult(const OptStringView&)>& call,
             const std::string& help = "")
        : _name(name), _call(call), _help(help) {}

    /// Returns the name of the RPC
    const std::string_view name() const { return _name; }

    /// Returns the help string of the RPC
    const std::string_view help() const { return _help; }

    /// Invoke the RPC
    RPCResult call(const OptStringView& value) const { return std::move(_call(value)); }

private:
    const std::string _name;
    const std::string _help;
    const std::function<RPCResult(const OptStringView&)>
        _call; // todo: it would be lovely to burry this heap greedy std::function
};

} // namespace kaskas::prompt
