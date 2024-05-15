#pragma once

// #include <AH/Arduino-Wrapper.h>
#include "kaskas/io/clock.hpp"

#include <DS3231-RTC.h>
#include <spine/core/debugging.hpp>
#include <spine/core/exception.hpp>
#include <spine/platform/hal.hpp>

#include <AH/STL/cstdint>

namespace kaskas::io::clock {

// threadsafe, not so much
class DS3231Clock final : Clock<DS3231Clock> {
public:
public:
    static void initialize() {
        auto ds3231 = get_instance();

        if (ds3231->is_initialized)
            return;
        DBG("Initializing DS3231Clock");

        ds3231->is_initialized = true;
        HAL::I2C::initialize();

        // set DS3231 options
        const auto notThatWeirdAMPMStuff = false;
        ds3231->ds3231.setClockMode(notThatWeirdAMPMStuff);

        if (is_ready()) {
            const auto n = now();
            DBGF("Clock is ready. The reported time is %u:%u @ %u:%u:%u", n.getHour(), n.getMinute(), n.getDay(),
                 n.getMonth(), n.getYear());
        } else {
            dbg::throw_exception(spn::core::assertion_error("Clock failed to initialize. Maybe set the time?"));
        }
    }
    static DateTime now() { return get_instance()->rtclib.now(); }
    static void set_time(const DateTime& datetime) { get_instance()->ds3231.setEpoch(datetime.getUnixTime()); }
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

} // namespace kaskas::io::clock

using Clock = kaskas::io::clock::DS3231Clock;