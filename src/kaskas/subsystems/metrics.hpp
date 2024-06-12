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

    void initialize() override {}

    void safe_shutdown(State state) override {}

    void handle_event(const Event& event) override {}

    std::unique_ptr<prompt::RPCRecipe> rpc_recipe() override {
        using namespace prompt;
        auto model = std::make_unique<RPCRecipe>(
            RPCRecipe("MOC", //
                      {
                          RPCModel("roVariable",
                                   [this](const OptStringView&) {
                                       DBGF("roVariable accessed");
                                       return RPCResult(std::to_string(roVariable));
                                   }),
                          RPCModel("rwVariable",
                                   [this](const OptStringView& s) {
                                       const auto s_str = s ? std::string{*s} : std::string();
                                       DBGF("rwVariable accessed with arg: {%s}", s_str.c_str());
                                       rwVariable = s ? std::stod(std::string(*s)) : rwVariable;
                                       return RPCResult(std::to_string(rwVariable));
                                   }), //
                          RPCModel("foo",
                                   [this](const OptStringView& s) {
                                       if (!s || s->length() == 0) {
                                           return RPCResult(RPCResult::State::BAD_INPUT);
                                       }
                                       const auto s_str = s ? std::string{*s} : std::string();
                                       DBGF("foo called with arg: '%s'", s_str.c_str());

                                       return RPCResult(std::to_string(foo(std::stod(s_str))));
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