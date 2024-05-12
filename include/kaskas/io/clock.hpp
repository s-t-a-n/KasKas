#pragma once

// #include <AH/Arduino-Wrapper.h>
#include <spine/core/debugging.hpp>
#include <spine/platform/hal.hpp>

#include <AH/STL/cstdint>

// todo: rip DateTime from DS3231 lib to Spine
#include <DS3231-RTC.h>
using DateTime = DS3231::DateTime;

namespace kaskas::io {

template<typename ClockImp>
class Clock {
public:
    using UnixTime = uint32_t;

public:
    static void initialize() { ClockImp::initialize(); }
    static DateTime now() { return ClockImp::now(); }
    static void set_time(const DateTime& datetime) { ClockImp::set_time(datetime); }
    static UnixTime epoch() { return ClockImp::epoch(); }
    static bool is_ready() { return ClockImp::is_ready(); }

private:
};

} // namespace kaskas::io
