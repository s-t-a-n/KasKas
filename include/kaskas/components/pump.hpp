#pragma once

#include "kaskas/components/relay.hpp"

#include <spine/core/exception.hpp>
#include <spine/core/timers.hpp>
#include <spine/eventsystem/eventsystem.hpp>
#include <spine/filter/EWMA.hpp>
#include <spine/platform/hal.hpp>

#include <AH/STL/cstdint>

using spn::core::Exception;
using spn::core::time::Timer;

struct Pump {
private:
    using Flowrate = spn::filter::EWMA<double>;

public:
    struct Config {
        Relay::Config pump_relay_cfg;
        Interrupt::Config interrupt_cfg;
        double ml_calibration_factor;
        time_ms reading_interval;
        time_s pump_timeout;
    };

public:
    explicit Pump(Config&& cfg)
        : _cfg(cfg), _pump(std::move(cfg.pump_relay_cfg)), _interrupt(std::move(cfg.interrupt_cfg)),
          _flowrate(Flowrate::Config((cfg.pump_timeout / cfg.reading_interval).raw() / 2)) {}

    void initialize() {
        _pump.initialize();
        _interrupt.initialize();
    }

    //    time_ms time_since_last_injection() { return _last_reading.timeSinceLast(false); }

    uint32_t ml_since_injection_start() const { return _ml; }
    time_ms time_since_injection_start() { return _pump_timer.timeSinceLast(false); }
    time_ms time_since_last_injection() { return _last_injection.timeSinceLast(false); }

    double flowrate_lm() { return _flowrate.value(); }

    uint32_t read_ml_injected() {
        const auto time_since_last_reading = _last_reading.timeSinceLast(true);
        const auto pulse_count = read_pulsecount();
        const auto flowrate_lm =
            ((_cfg.reading_interval.raw() / time_since_last_reading.raw()) * pulse_count) / _cfg.ml_calibration_factor;
        _flowrate.new_reading(flowrate_lm);
        // Divide the flow rate in litres/minute by 60 to determine how many litres have
        // passed through the sensor in this interval, then multiply by 1000 to
        // convert to millilitres.
        const auto ml_since_last = (flowrate_lm / 60) * _cfg.reading_interval.raw();
        _ml += ml_since_last;
        return ml_since_last;
    }

    void start_injection() {
        _pump_timer.reset();
        _last_reading.reset();
        _last_injection.reset();
        _flowrate.reset_to(0);
        _ml = 0;

        reset_interrupt_counter();
        attach_interrupt();
        _pump.set_state(DigitalState::ON);
    }

    void stop_injection() {
        _pump.set_state(DigitalState::OFF);
        detach_interrupt();
        _lifetime_ml += _ml;
        _flowrate.reset_to(0);
        _last_injection.reset();
    }

    uint32_t lifetime_pumped_ml() const { return _lifetime_ml; }

private:
    // interrupt
    void attach_interrupt();
    void detach_interrupt();
    static uint32_t interrupt_counter();
    static void reset_interrupt_counter();

    uint32_t read_pulsecount() {
        // Because this loop may not complete in exactly _cfg.pump_check_interval intervals we calculate
        // the number of milliseconds that have passed since the last execution and use
        // that to scale the output. We also apply the calibrationFactor to scale the output
        // based on the number of pulses per second per units of measure (litres/minute in
        // this case) coming from the sensor.
        detach_interrupt();
        const auto pulse_count = interrupt_counter();
        reset_interrupt_counter();
        attach_interrupt();

        return pulse_count;
    }

private:
    const Config _cfg;

    Relay _pump;
    Interrupt _interrupt;

    spn::filter::EWMA<double> _flowrate; // tracks flowrate in liters per minute

    uint32_t _ml = 0; // tracks ml since start of last injection
    uint32_t _lifetime_ml = 0; // tracks ml since boot up

    Timer _pump_timer; // tracks time since start of injection
    Timer _last_reading; // tracks time since last interrupt reading
    Timer _last_injection; // tracks time since last injection
};