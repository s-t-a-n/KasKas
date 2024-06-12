#pragma once

#include "kaskas/io/peripheral.hpp"
#include "kaskas/io/providers/analogue.hpp"
#include "kaskas/io/providers/clock.hpp"
#include "kaskas/io/providers/non_volatile_memory.hpp"

#include <DS3231-RTC.h>
#include <spine/core/debugging.hpp>
#include <spine/core/exception.hpp>
#include <spine/filter/implementations/bandpass.hpp>
#include <spine/platform/hal.hpp>

namespace kaskas::io::clock {

class DS3231Clock final : public Peripheral {
public:
    struct Config {
        time_ms update_interval = time_s(1);
    };

    explicit DS3231Clock(const Config&& cfg) : Peripheral(cfg.update_interval), _cfg(cfg) {}

    ~DS3231Clock() override = default;

    void initialize() override {
        // DBG("Initializing DS3231Clock");
        HAL::I2C::initialize();

        // set DS3231's clockmode to 24h, as basic civilisation demands
        _ds3231.setClockMode(false);

        update();

        if (is_ready()) {
            const auto n = now();
            DBGF("DS3231Clock initialized. The reported time is %u:%u @ %u:%u:%u", n.getHour(), n.getMinute(),
                 n.getDay(), n.getMonth(), n.getYear());
        } else {
            spn::throw_exception(spn::assertion_error("DS3231Clock failed to initialize. Maybe set the time?"));
        }
    }

    void update() override {
        assert(is_ready());
        _now = DS3231::RTClib::now();
    }
    void safe_shutdown(bool critical) override {}

    const DateTime& now() const { return _now; }
    void set_time(const DateTime& datetime) { _ds3231.setEpoch(datetime.getUnixTime()); }
    time_t epoch() { return _now.getUnixTime(); }
    bool is_ready() { return _ds3231.oscillatorCheck(); }

    AnalogueSensor temperature_provider() {
        return {[this]() { return this->_ds3231.getTemperature(); }};
    }

    Clock clock_provider() {
        const auto map = Clock::FunctionMap{.now_f = [this]() { return this->now(); },
                                            .settime_f = [this](const DateTime& datetime) { this->set_time(datetime); },
                                            .epoch_f = [this]() { return this->epoch(); },
                                            .isready_f = [this]() { return this->is_ready(); }};
        return {std::move(map)};
    }

    NonVolatileMemory non_volatile_memory_provider() { return NonVolatileMemory{}; }

private:
    const Config _cfg;

    DS3231::DS3231 _ds3231;

    DateTime _now;
};

} // namespace kaskas::io::clock

// using Clock = kaskas::io::clock::DS3231Clock;