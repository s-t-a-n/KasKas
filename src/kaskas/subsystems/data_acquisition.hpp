#pragma once

#include "kaskas/component.hpp"
#include "kaskas/prompt/prompt.hpp"
#include "kaskas/subsystems/climatecontrol.hpp"

#include <spine/core/meta/enum.hpp>

#include <charconv>
#include <cstdint>
#include <iterator>
#include <numeric>

namespace kaskas::component {

namespace meta = spn::core::meta;

class DataAcquisition : public Component {
public:
    struct Config {
        time_s initial_warm_up_time = time_s(30); // don't allow timeseries access immediately after startup
        std::initializer_list<DataProviders> active_dataproviders; // dataproviders for which to print timeseries
    };

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

    void safe_shutdown(State state) override {
        DBG("DAQ: Shutting down");
        _status.Flags.warmed_up = false;
    }

    void handle_event(const Event& event) override {
        switch (static_cast<Events>(event.id())) {
        case Events::DAQWarmedUp: _status.Flags.warmed_up = true; break;
        case Events::DAQTainted: _status.Flags.tainted = true; break;
        default: spn_assert(!"Event was not handled!"); break;
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
                                       if (!is_warmed_up())
                                           return RPCResult("Data acquisition has not warmed up yet",
                                                            RPCResult::Status::BAD_RESULT);
                                       return RPCResult(timeseries_as_string());
                                   }),
                      }));
        return std::move(model);
    }
    void sideload_providers(io::VirtualStackFactory& ssf) override {}

public:
    bool is_warmed_up() const { return _status.Flags.warmed_up; }

    std::string datasources_as_string() {
        std::string fields;

        size_t reserved_size = 0;
        for (auto datasource : _cfg.active_dataproviders) {
            const auto name = magic_enum::enum_name(datasource);
            reserved_size += name.length() + 1; // 1 for divider
        }
        reserved_size += 1; // one for newline
        fields.reserve(reserved_size);
        reserved_size = fields.capacity(); // for sanity checks below

        for (auto it = _cfg.active_dataproviders.begin(); it != _cfg.active_dataproviders.end(); ++it) {
            const auto& datasource = *it;
            const auto name = magic_enum::enum_name(datasource);
            fields += name.data();
            if (std::next(it) != _cfg.active_dataproviders.end()) fields += prompt::Dialect::VALUE_SEPARATOR;
        }

        spn_assert(fields.capacity() == reserved_size); // no reallocation
        return std::move(fields);
    }

    std::string timeseries_as_string() {
        auto timeseries_repr = [&](std::string* s, bool length_only) -> size_t {
            size_t len = 0;
            auto put = [&](std::string_view i) {
                if (!length_only) {
                    *s += i;
                }
                len += i.size();
            };

            std::array<char, 32> buffer{};
            for (auto it = _cfg.active_dataproviders.begin(); it != _cfg.active_dataproviders.end(); ++it) {
                const auto value = _hws.analog_sensor(meta::ENUM_IDX(*it)).value();
                int written = std::snprintf(buffer.data(), buffer.size(), "%.3f", value);
                spn_expect(written > 0 && written < buffer.size());
                if (written > 0 && written < buffer.size()) put(std::string_view(buffer.data(), written));
                if (std::next(it) != _cfg.active_dataproviders.end()) put(prompt::Dialect::VALUE_SEPARATOR);
            }
            return len;
        };

        // First pass to calculate total length
        size_t reserved_length = timeseries_repr(nullptr, true);

        // Reserve capacity to avoid multiple allocations
        std::string timeseries{};
        timeseries.reserve(reserved_length);
        reserved_length = timeseries.capacity(); // for sanity checks below

        // Second pass to build the string
        timeseries_repr(&timeseries, false);

        spn_assert(timeseries.size() <= reserved_length); // no reallocation
        spn_assert(timeseries.capacity() == reserved_length); // no reallocation

        return timeseries;
    }

private:
    const Config _cfg;
    Status _status;
};
} // namespace kaskas::component
