#pragma once

#include "kaskas/component.hpp"
#include "kaskas/prompt/prompt.hpp"
#include "kaskas/subsystems/climatecontrol.hpp"

namespace kaskas::component {
class Metrics : public Component {
public:
    struct Config {
        time_ms default_dump_interval = time_s(1);
    };

    Metrics(io::HardwareStack& hws, const Config& cfg) : Metrics(hws, nullptr, cfg) {}
    Metrics(io::HardwareStack& hws, EventSystem* evsys, const Config& cfg)
        : Component(evsys, hws), _cfg(std::move(cfg)), _dump_interval(_cfg.default_dump_interval) {}

    void initialize() override {
        evsys()->attach(Events::MetricsStartDatadump, this);
        evsys()->attach(Events::MetricsFollowUp, this);
        evsys()->attach(Events::MetricsStopDatadump, this);
    }

    void safe_shutdown(State state) override {}

    void handle_event(const Event& event) override {
        switch (static_cast<Events>(event.id())) {
        case Events::MetricsStartDatadump: {
            DBG("Metrics: StartDatadump");
            if (!_data_dump_enabled) {
                _data_dump_enabled = true;
                evsys()->schedule(evsys()->event(Events::MetricsFollowUp, _dump_interval, Event::Data()));
            } else {
                DBGF("start_data_dump: Datadump already running!");
            }
            break;
        }
        case Events::MetricsFollowUp: {
            // DBG("Metrics: FollowUp");
            if (_data_dump_enabled) {
                // dump_data();
                const auto metrics = metrics_as_string();
                HAL::print(metrics.c_str());
                evsys()->schedule(evsys()->event(Events::MetricsFollowUp, _dump_interval, Event::Data()));
            }
            break;
        }
        case Events::MetricsStopDatadump: {
            DBG("Metrics: StopDatadump");
            _data_dump_enabled = false;
            break;
        }
        default: assert(!"Event was not handled!"); break;
        };
    }

    std::unique_ptr<prompt::RPCRecipe> rpc_recipe() override {
        using namespace prompt;
        auto model = std::make_unique<RPCRecipe>(RPCRecipe(
            "MTC", //
            {
                RPCModel("startDatadump",
                         [this](const OptStringView&) {
                             // announce_data();
                             start_data_dump();
                             return RPCResult{RPCResult::State::OK};
                         }),
                RPCModel("stopDatadump",
                         [this](const OptStringView&) {
                             stop_data_dump();
                             return RPCResult{RPCResult::State::OK};
                         }),
                RPCModel("getFields", [this](const OptStringView&) { return RPCResult(datasources_as_string()); }),
                RPCModel("getMetrics", [this](const OptStringView&) { return RPCResult(metrics_as_string()); }),
                RPCModel("setDatadumpIntervalS",
                         [this](const OptStringView& s) {
                             if (!s)
                                 return RPCResult{RPCResult::State::BAD_INPUT};
                             const auto interval = std::stoi(*s);
                             if (interval <= 0)
                                 return RPCResult{"Interval cannot be below zero", RPCResult::State::BAD_INPUT};
                             set_data_dump_interval(time_s(interval));
                             return RPCResult{RPCResult::State::OK};
                         }),
            }));
        return std::move(model);
    }
    void sideload_providers(io::VirtualStackFactory& ssf) override {}

    // todo: unhardcode this
    static constexpr auto datasources = {DataProviders::CLIMATE_TEMP, //
                                         DataProviders::HEATING_SURFACE_TEMP, //
                                         DataProviders::HEATING_SETPOINT, //
                                         DataProviders::AMBIENT_TEMP, //
                                         DataProviders::CLIMATE_HUMIDITY, //
                                         DataProviders::SOIL_MOISTURE, //
                                         DataProviders::CLIMATE_FAN, //
                                         DataProviders::SOIL_MOISTURE_SETPOINT};

    std::string datasources_as_string() {
        std::string fields;

        size_t alloc_size = 0;
        for (auto datasource : datasources) {
            const auto name = magic_enum::enum_name(datasource);
            alloc_size += name.length() + 1; // 1 for divider
        }
        alloc_size += 1; // one for newline
        fields.reserve(alloc_size);

        for (auto datasource : datasources) {
            const auto name = magic_enum::enum_name(datasource);
            fields += name.data();
            fields += prompt::Dialect::VALUE_SEPARATOR;
        }
        fields += "\n";
        return std::move(fields);
    }

    std::string metrics_as_string() {
        std::string metrics;
        metrics.reserve(120);
        for (auto datasource : datasources) {
            const auto value = _hws.analog_sensor(ENUM_IDX(datasource)).value();
            metrics += std::to_string(value);
            metrics += prompt::Dialect::VALUE_SEPARATOR;
        }
        metrics += "\n";
        return std::move(metrics);
    }

    void start_data_dump() { evsys()->trigger(evsys()->event(Events::MetricsStartDatadump, time_s(0), Event::Data())); }
    void stop_data_dump() { evsys()->trigger(evsys()->event(Events::MetricsStopDatadump, time_s(0), Event::Data())); }
    void set_data_dump_interval(const time_s interval) {
        assert(interval > time_s(0));
        _dump_interval = interval;
    }
    bool is_dumping_data() const { return _data_dump_enabled; }

private:
    const Config _cfg;

    bool _data_dump_enabled = false;

    time_s _dump_interval;
};
} // namespace kaskas::component