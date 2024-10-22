#pragma once

#include "kaskas/io/peripherals/relay.hpp"

#include <spine/core/exception.hpp>
#include <spine/core/types.hpp>
#include <spine/eventsystem/eventsystem.hpp>
#include <spine/filter/implementations/ewma.hpp>
#include <spine/platform/hal.hpp>
#include <spine/structure/time/timers.hpp>

#include <cstdint>

namespace kaskas::io {

struct Pump {
private:
    using Flowrate = spn::filter::EWMA<float>;
    using LogicalState = spn::core::LogicalState;

public:
    struct Config {
        io::HardwareStack::Idx pump_actuator_idx;
        Interrupt::Config interrupt_cfg;
        float ml_pulse_calibration; // experimentally found flow sensor calibration factor
        k_time_ms reading_interval;
        k_time_ms pump_timeout;
        float minimal_pump_flowrate = 0.1;
    };

    union Status {
        struct BitFlags {
            bool out_of_fluid : 1;
            bool has_injected_since_start : 1;
            uint8_t unused : 6;
        } flags;
        uint8_t status = 0;
    };

public:
    explicit Pump(io::HardwareStack& hws, Config&& cfg)
        : _cfg(cfg), _status({}), _pump(hws.digital_actuator(_cfg.pump_actuator_idx)),
          _interrupt(std::move(cfg.interrupt_cfg)),
          _flowrate(Flowrate::Config{.K = (cfg.pump_timeout / cfg.reading_interval).raw<float>() / 2.0f}) {}

    void initialize() { _interrupt.initialize(); }

    void safe_shutdown() {
        DBG("Pump: Shutting down");
        HAL::delay(k_time_ms(100));
        stop_injection();
    }

    bool is_out_of_fluid() const { return _status.flags.out_of_fluid; }
    bool is_injecting() const { return _pump.state() == LogicalState::ON; }
    uint32_t ml_since_injection_start() const { return _ml; }
    k_time_ms time_since_injection_start() {
        return _status.flags.has_injected_since_start ? _pump_timer.time_since_last(false) : k_time_ms(0);
    }
    k_time_ms time_since_last_injection() {
        return _status.flags.has_injected_since_start ? _last_injection.time_since_last(false) : k_time_ms(0);
    }
    uint32_t lifetime_pumped_ml() const { return _lifetime_ml; }
    float flowrate_lm() const { return _flowrate.value(); }

    void start_injection(uint16_t amount_ml = 0) {
        _status.flags.has_injected_since_start = true;
        _pump_timer.reset();
        _last_reading.reset();
        _last_injection.reset();
        _flowrate.reset_to(0);
        _target_ml = amount_ml;
        _ml = 0;
        reset_interrupt_counter();
        attach_interrupt();
        _pump.set_state(LogicalState::ON);
    }

    void update() {
        if (!is_injecting()) return;
        track_injection();

        if (time_since_injection_start() > _cfg.pump_timeout && flowrate_lm() < _cfg.minimal_pump_flowrate) {
            DBG("Pump: not enough pumped within space of time. Flowrate: %.2f, minimal flowrate: %.2f", flowrate_lm(),
                _cfg.minimal_pump_flowrate);
            _status.flags.out_of_fluid = true;
            stop_injection();
            return;
        }

        if (_target_ml > 0 && _ml >= _target_ml) {
            stop_injection();
        }
    }

    void stop_injection() {
        _pump.set_state(LogicalState::OFF);
        detach_interrupt();
        _lifetime_ml += _ml;
        _target_ml = 0;
        _flowrate.reset_to(0);
        _last_injection.reset();
    }

private:
    using Exception = spn::core::Exception;
    using Timer = spn::structure::time::Timer;

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

    uint32_t track_injection() {
        const auto time_since_last_reading = _last_reading.time_since_last(true);
        const auto pulse_count = read_pulsecount();
        spn_assert(time_since_last_reading.raw() != 0);
        spn_assert(_cfg.ml_pulse_calibration != 0);
        const auto flowrate_lm =
            ((_cfg.reading_interval / time_since_last_reading) * pulse_count).raw() / _cfg.ml_pulse_calibration;
        _flowrate.new_sample(flowrate_lm);
        // Divide the flow rate in litres/minute by 60 to determine how many litres have
        // passed through the sensor in this interval, then multiply by the milliseconds passed to
        // convert to millilitres.
        const auto ml_since_last = (flowrate_lm / 60) * time_since_last_reading.raw<float>();
        _ml += ml_since_last;

        DBG("ML injected: pulses: %i, ml_since_last: %f, ml total: %i, flowrate: %f", pulse_count, ml_since_last, _ml,
            _flowrate.value())
        return ml_since_last;
    }

private:
    const Config _cfg;
    Status _status;

    io::DigitalActuator _pump;
    Interrupt _interrupt;

    spn::filter::EWMA<float> _flowrate; // tracks flowrate in liters per minute

    uint32_t _target_ml = 0; // the target amount before `update()` stops injection
    uint32_t _ml = 0; // tracks ml since start of last injection
    uint32_t _lifetime_ml = 0; // tracks ml since boot up

    Timer _pump_timer; // tracks time since start of injection
    Timer _last_reading; // tracks time since last interrupt reading
    Timer _last_injection; // tracks time since last injection
};
} // namespace kaskas::io
