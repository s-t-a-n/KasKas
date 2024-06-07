#pragma once

// #include <AH/Arduino-Wrapper.h>
#include <spine/core/debugging.hpp>
#include <spine/platform/hal.hpp>

#include <AH/STL/cstdint>

// todo: rip DateTime from DS3231 lib to Spine
#include "kaskas/io/provider.hpp"

#include <DS3231-RTC.h>
using DateTime = DS3231::DateTime;

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

private:
    const FunctionMap _map;
};

} // namespace kaskas::io
