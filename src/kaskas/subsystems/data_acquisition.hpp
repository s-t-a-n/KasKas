#pragma once

#include "kaskas/component.hpp"
#include "kaskas/prompt/prompt.hpp"
#include "kaskas/subsystems/climatecontrol.hpp"

#include <stdint.h>

namespace kaskas::component {
class DataAcquisition : public Component {
public:
    struct Config {
        time_s initial_warm_up_time = time_s(30); // don't allow timeseries access immediately after startup
        std::initializer_list<DataProviders> active_dataproviders; // dataproviders for which to print timeseries
    };

    // union Status {
    //     struct BitFlags {
    //         bool metrics_ready : 1;
    //         bool metrics_tainted : 1;
    //         bool metrics_lagging : 1;
    //         uint8_t unused : 6;
    //     } Flags;
    //     uint8_t status;
    // };
    union Status {
        struct BitFlags {
            bool warmed_up : 1;
            bool tainted : 1;
            uint8_t unused : 6;
        } Flags;
        uint8_t status = 0;
    };

    DataAcquisition(io::HardwareStack& hws, const Config& cfg) : DataAcquisition(hws, nullptr, cfg) {}
    DataAcquisition(io::HardwareStack& hws, EventSystem* evsys, const Config& cfg)
        : Component(evsys, hws), _cfg(std::move(cfg)), _status({}) {}

    void initialize() override {
        evsys()->attach(Events::DAQWarmedUp, this);
        evsys()->attach(Events::DAQTainted, this);
        evsys()->schedule(evsys()->event(Events::DAQWarmedUp, time_s(_cfg.initial_warm_up_time)));
    }

    void safe_shutdown(State state) override {}

    void handle_event(const Event& event) override {
        switch (static_cast<Events>(event.id())) {
        case Events::DAQWarmedUp: _status.Flags.warmed_up = true; break;
        case Events::DAQTainted: _status.Flags.tainted = true; break;
        default: assert(!"Event was not handled!"); break;
        }
    }

    std::unique_ptr<prompt::RPCRecipe> rpc_recipe() override {
        using namespace prompt;
        auto model = std::make_unique<RPCRecipe>(
            RPCRecipe("DAQ", //
                      {
                          RPCModel("getTimeSeriesColumns",
                                   [this](const OptStringView&) { return RPCResult(datasources_as_string()); }),
                          RPCModel("getTimeSeries",
                                   [this](const OptStringView&) {
                                       if (!is_ready())
                                           return RPCResult("Data acquisition is not ready for collection",
                                                            RPCResult::State::BAD_RESULT);
                                       return RPCResult(timeseries_as_string());
                                   }),
                      }));
        return std::move(model);
    }
    void sideload_providers(io::VirtualStackFactory& ssf) override {}

public:
    bool is_ready() const { return _status == Status::READY; }
    bool is_warmed_up() const { return _status == Status::READY; }

    std::string datasources_as_string() {
        std::string fields;

        size_t alloc_size = 0;
        for (auto datasource : _cfg.active_dataproviders) {
            const auto name = magic_enum::enum_name(datasource);
            alloc_size += name.length() + 1; // 1 for divider
        }
        alloc_size += 1; // one for newline
        fields.reserve(alloc_size);

        for (auto datasource : _cfg.active_dataproviders) {
            const auto name = magic_enum::enum_name(datasource);
            fields += name.data();
            fields += prompt::Dialect::VALUE_SEPARATOR;
        }
        fields += "\n";
        return std::move(fields);
    }

    std::string timeseries_as_string() {
        std::string timeseries;
        timeseries.reserve(120);
        for (auto datasource : _cfg.active_dataproviders) {
            const auto value = _hws.analog_sensor(ENUM_IDX(datasource)).value();
            timeseries += std::to_string(value);
            timeseries += prompt::Dialect::VALUE_SEPARATOR;
        }
        timeseries += "\n";
        return std::move(timeseries);
    }

private:
    const Config _cfg;
    Status _status;
};
} // namespace kaskas::component