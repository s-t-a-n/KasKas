#pragma once

#include <AH/Arduino-Wrapper.h>
#include <DS3231-RTC.h>
#include <spine/core/debugging.hpp>
#include <spine/platform/hal.hpp>

#include <AH/STL/cstdint>

// todo: integrate DateTime from DS3231 lib to Spine
using DateTime = DS3231::DateTime;

namespace kaskas::components::clock {

template<typename ClockImp>
class Clock {
public:
    using UnixTime = uint32_t;

public:
    static void initialize() { ClockImp::initialize(); }
    static DateTime now() { return ClockImp::now(); }
    static void set_time(DateTime& datetime) { ClockImp::set_time(); }
    static UnixTime epoch() { return ClockImp::epoch(); }
    static bool is_ready() { return ClockImp::is_ready(); }

private:
};

// threadsafe, not so much
class DS3231Clock final : Clock<DS3231Clock> {
public:
public:
    static void initialize() {
        auto ds3231 = get_instance();

        if (ds3231->is_initialized)
            return;
        DBG("initializing DS3231Clock");

        ds3231->is_initialized = true;
        HAL::I2C::initialize();

        // set DS3231 options
        const auto notThatWeirdAMPMStuff = false;
        ds3231->ds3231.setClockMode(notThatWeirdAMPMStuff);
    }
    static DateTime now() { return get_instance()->rtclib.now(); }
    static void set_time(DateTime& datetime) { get_instance()->ds3231.setEpoch(datetime.getUnixTime()); }
    static UnixTime epoch() { return get_instance()->rtclib.now().getUnixTime(); }

    static bool is_ready() { return get_instance()->ds3231.oscillatorCheck(); }

    struct ds3231_singleton {
        bool is_initialized = false;
        DS3231::DS3231 ds3231;
        DS3231::RTClib rtclib;
    };

private:
    static ds3231_singleton* ds3231_singleton;

    static struct ds3231_singleton* get_instance() {
        if (ds3231_singleton == nullptr) {
            ds3231_singleton = new struct ds3231_singleton();
        }
        return ds3231_singleton;
    }
};

} // namespace kaskas::components::clock

using Clock = kaskas::components::clock::DS3231Clock;
