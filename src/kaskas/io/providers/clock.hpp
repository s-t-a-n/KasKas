#pragma once

#include "kaskas/io/provider.hpp"

#include <spine/core/debugging.hpp>
#include <spine/platform/hal.hpp>
#include <spine/structure/time/datetime.hpp>
#include <spine/structure/units/time.hpp>

namespace kaskas::io {

class Clock : public Provider {
public:
    using UnixTime = time_t;

    struct FunctionMap {
        const std::function<DateTime()> now_f;
        const std::function<void(DateTime)> settime_f;
        const std::function<UnixTime()> epoch_f;
        const std::function<bool()> isready_f;
    };

    Clock(const FunctionMap&& map) : _map(std::move(map)) {}

public:
    DateTime now() const { return _map.now_f(); }
    void set_time(const DateTime& datetime) const { _map.settime_f(datetime); }
    UnixTime epoch() const { return _map.epoch_f(); }
    bool is_ready() const { return _map.isready_f(); }

    k_time_s time_until_next_occurence(k_time_s time_of_day) const {
        const auto n = now();
        const auto hours = k_time_h(time_of_day);
        const auto minutes = k_time_m(time_of_day - hours);
        const auto seconds = k_time_s(time_of_day - hours - minutes);

        auto today_at_tod = [n, hours, minutes, seconds]() -> DateTime {
            return DateTime(n.getYear(), n.getMonth(), n.getDay(), hours.raw<char>(), minutes.raw<char>(),
                            seconds.raw<char>(), n.getWeekDay(), n.getYearDay());
        };
        auto tomorrow_at_tod = [n, hours, minutes, seconds]() -> DateTime {
            auto t = DateTime(n.getUnixTime() + k_time_s(k_time_d(1)).raw());
            return DateTime(t.getYear(), t.getMonth(), t.getDay(), hours.raw<char>(), minutes.raw<char>(),
                            seconds.raw<char>(), t.getWeekDay(), t.getYearDay());
        };

        if (n.getUnixTime() < today_at_tod().getUnixTime()) // is later today
            return k_time_s(today_at_tod().getUnixTime() - n.getUnixTime());
        return k_time_s(tomorrow_at_tod().getUnixTime() - n.getUnixTime()); // is tomorrow
    }

    std::unique_ptr<prompt::RPCRecipe> rpc_recipe(const std::string_view& recipe_name, const std::string_view& root) {
        return {};
    }

private:
    const FunctionMap _map;
};

} // namespace kaskas::io
