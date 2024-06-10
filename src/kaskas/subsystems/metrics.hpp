#pragma once

#include "kaskas/component.hpp"
#include "kaskas/prompt/prompt.hpp"
#include "kaskas/subsystems/climatecontrol.hpp"

namespace kaskas::component {
class Metrics : public Component {
public:
    struct Config {};

    Metrics(io::HardwareStack& hws, const Config& cfg) : Metrics(hws, nullptr, cfg) {}
    Metrics(io::HardwareStack& hws, EventSystem* evsys, const Config& cfg)
        : Component(evsys, hws), _cfg(std::move(cfg)) {}

    std::unique_ptr<prompt::RPCRecipe> rpc_recipe() override {
        using namespace prompt;
        using kaskas::prompt::OptStringView;

        using spn::structure::Array;

        // RPCModel{._command = "", ._rpcs = {ROVariable("ROVariable", [this](const OptString&) {
        //                              return RPCResult(std::to_string(roVariable));
        //                          })}};

        // because of the use of lambda's, this must be allocated dynamically, a pity
        auto model = std::make_unique<RPCRecipe>(RPCRecipe(
            "MTC", //
            {
                ROVariable("roVariable", [this](const OptString&) { return RPCResult(std::to_string(roVariable)); }),
                RWVariable("rwVariable",
                           [this](const OptString& s) {
                               return RPCResult(std::to_string(rwVariable = s ? std::stod(*s) : rwVariable));
                           }), //
                FunctionCall("foo",
                             [this](const OptString& s) {
                                 if (!s)
                                     return RPCResult(RPCResult::State::BAD_INPUT);
                                 return RPCResult(std::to_string(foo(std::stod(*s))));
                             }),
            }));
        return std::move(model);
    }

private:
    const Config _cfg;

    const double roVariable = 42;
    double rwVariable = 42;

    double foo(double variable) { return roVariable + rwVariable + variable; }
};
} // namespace kaskas::component