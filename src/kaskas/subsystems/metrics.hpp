#pragma once

#include "kaskas/component.hpp"
#include "kaskas/prompt/prompt.hpp"
#include "kaskas/subsystems/climatecontrol.hpp"

namespace kaskas::component {
class Metrics : public Component {
public:
    struct Config {
        time_ms dump_interval = time_s(1);
    };

    Metrics(io::HardwareStack& hws, const Config& cfg) : Metrics(hws, nullptr, cfg) {}
    Metrics(io::HardwareStack& hws, EventSystem* evsys, const Config& cfg)
        : Component(evsys, hws), _cfg(std::move(cfg)) {}

    void initialize() override {
        evsys()->attach(Events::MetricsStartDatadump, this);
        evsys()->attach(Events::MetricsFollowUp, this);
        evsys()->attach(Events::MetricsStopDatadump, this);
    }

    void safe_shutdown(State state) override {}

    void handle_event(const Event& event) override {
        //
        switch (static_cast<Events>(event.id())) {
        case Events::MetricsStartDatadump: {
            //
            DBG("Metrics: StartDatadump");
            _data_dump_enabled = true;
            evsys()->schedule(evsys()->event(Events::MetricsFollowUp, _cfg.dump_interval, Event::Data()));
            break;
        }
        case Events::MetricsFollowUp: {
            //
            // DBG("Metrics: FollowUp");
            dump_data();
            if (_data_dump_enabled) {
                evsys()->schedule(evsys()->event(Events::MetricsFollowUp, _cfg.dump_interval, Event::Data()));
            }
            break;
        }
        case Events::MetricsStopDatadump: {
            //
            DBG("Metrics: StopDatadump");
            _data_dump_enabled = false;
            break;
        }
        default: assert(!"Event was not handled!"); break;
        };
    }

    std::unique_ptr<prompt::RPCRecipe> rpc_recipe() override {
        using namespace prompt;
        auto model = std::make_unique<RPCRecipe>(RPCRecipe("MTC", //
                                                           {
                                                               RPCModel("startDatadump",
                                                                        [this](const OptStringView&) {
                                                                            start_data_dump();
                                                                            return RPCResult{RPCResult::State::OK};
                                                                        }),
                                                               RPCModel("stopDatadump",
                                                                        [this](const OptStringView&) {
                                                                            stop_data_dump();
                                                                            return RPCResult{RPCResult::State::OK};
                                                                        }),
                                                           }));
        return std::move(model);
    }

    // todo: unhardcode this
    static constexpr auto datasources = {Providers::CLIMATE_TEMP, //
                                         Providers::HEATER_SURFACE_TEMP, //
                                         Providers::OUTSIDE_TEMP, //
                                         Providers::CLIMATE_HUMIDITY, //
                                         Providers::SOIL_MOISTURE};

    void announce_data() {
        for (auto datasource : datasources) {
            const auto name = magic_enum::enum_name(datasource);
            HAL::print(name.data());
            HAL::print("|");
        }
        HAL::println("");
    }

    void dump_data() {
        for (auto datasource : datasources) {
            const auto value = _hws.analog_sensor(datasource).value();
            HAL::print(value);
            HAL::print("|");
        }
        HAL::println("");
    }

    void start_data_dump() {
        announce_data();
        evsys()->trigger(evsys()->event(Events::MetricsStartDatadump, time_s(0), Event::Data()));
    }

    void stop_data_dump() { evsys()->trigger(evsys()->event(Events::MetricsStopDatadump, time_s(0), Event::Data())); }
    bool is_dumping_data() const { return _data_dump_enabled; }

private:
    const Config _cfg;

    bool _data_dump_enabled = false;
};
} // namespace kaskas::component