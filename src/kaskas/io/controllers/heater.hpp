#pragma once

#include "kaskas/io/hardware_stack.hpp"

#include <magic_enum/magic_enum.hpp>
#include <spine/controller/pid.hpp>
#include <spine/core/debugging.hpp>
#include <spine/core/timers.hpp>
#include <spine/filter/implementations/bandpass.hpp>
#include <spine/filter/implementations/ewma.hpp>
#include <spine/io/sensor.hpp>
#include <spine/platform/hal.hpp>

#include <set>

namespace kaskas::io {

using spn::controller::PID;
using spn::core::time::AlarmTimer;
using spn::core::time::IntervalTimer;
using spn::core::time::Timer;
// using spn::filter::EWMA;

// todo: put this in a configuration file

constexpr double MIN_AMBIENT_TEMPERATURE = 12.0;
constexpr double MAX_AMBIENT_TEMPERATURE = 40.0;

constexpr double MIN_INSIDE_TEMPERATURE = 12.0;
constexpr double MAX_INSIDE_TEMPERATURE = 40.0;

constexpr double MIN_SURFACE_TEMPERATURE = 12.0;
constexpr double MAX_SURFACE_TEMPERATURE = 50.0;

// maximal error from setpoint in degrees that is allowed to happen for timewindow TRP_STABLE_TIMEWINDOW until thermal
// runaway is considered
constexpr double TRP_STABLE_HYSTERESIS_C = 4.0;
constexpr int TRP_STABLE_TIMEWINDOW = 40;

// minimal change in degrees within timewindow TRP_CHANGING_TIMEWINDOW
constexpr double TRP_CHANGING_MINIMAL_DELTA_C = 4.0;
constexpr double TRP_CHANGING_TIMEWINDOW = 40;

class Heater {
public:
    enum class State { UNDEFINED, IDLE, COOLING, HEATING, STEADY_STATE, THERMAL_RUN_AWAY };
    static constexpr std::string_view as_stringview(State state) noexcept { return magic_enum::enum_name(state); }
    enum class TemperatureSource { UNDEFINED, SURFACE, CLIMATE };

    using Value = double;

    // todo: move this into a generic Spine class that monitors using an expected envelope
    class ThermalRunAway {
    public:
        // todo: make an autotuner to find limits of normal usage experimentally
        struct Config {
            // maximal error from setpoint in degrees that is allowed to happen for timewindow stable_timewindow until
            // thermal runaway is considered
            // double stable_hysteresis_c = 1.0;
            time_s stable_timewindow = time_m(10);

            // minimal change in degrees within timewindow changing_timewindow
            double heating_minimal_rising_c = 0.1;
            double heating_minimal_dropping_c = 0.01;
            time_s heating_timewindow = time_m(20);

            bool guard_stable_state = true;
            bool guard_changing_state = true;
        };

        ThermalRunAway(const Config&& cfg)
            : _cfg(std::move(cfg)), _current_state_duration(Timer{}), _heating_time_window(_cfg.heating_timewindow) {}

        void update(State state, double temperature) {
            if (state != _current_state) { // heater changed states
                _last_state = _current_state;
                _current_state = state;
                reset_timers();
                _temperature_time_window = temperature;
                return;
            }

            if (_current_state == State::HEATING) {
                if (_cfg.guard_stable_state and _last_state == State::STEADY_STATE) { // heater came out of steady state
                    if (time_s(_current_state_duration.timeSinceLast(false))
                        > _cfg.stable_timewindow) { // heater went outside of steady state for too long
                        LOG("Heater went outside of steady state for too long, triggering run away! (time expired: "
                            "%is, timewindow: %is, current temperature %.2fC)",
                            time_s(_current_state_duration.timeSinceLast(false)).printable(),
                            time_s(_cfg.stable_timewindow).printable(), temperature)
                        _is_runaway = true;
                    }
                    return;
                }

                if (_cfg.guard_changing_state and _heating_time_window.expired()) {
                    if (temperature < _setpoint) { // heater is trying to rise temperature
                        const auto error = temperature - _temperature_time_window;
                        if (error < _cfg.heating_minimal_rising_c) { // temperature didnt rise fast enough
                            LOG("Temperature didnt rise fast enough, triggering run away! (delta:%.2fC, temperature: "
                                "%.2fC)",
                                error, temperature);
                            _is_runaway = true;
                        }
                        _temperature_time_window = temperature;
                    }

                    if (temperature > _setpoint) { // heater should be dropping temperature
                        const auto error = _temperature_time_window - temperature;
                        if (error < _cfg.heating_minimal_dropping_c) { // temperature didn't drop fast enough
                            LOG("Temperature didnt drop fast enough, triggering run away! (delta:%.2fC, temperature: "
                                "%.2fC)",
                                error, temperature);
                            _is_runaway = true;
                        }
                        _temperature_time_window = temperature;
                    }
                }
            }
        }

        void adjust_setpoint(double setpoint) {
            _setpoint = setpoint;
            reset_timers();
            _current_state = State::UNDEFINED;
            _last_state = State::UNDEFINED;
        }

        bool is_runaway() const { return _is_runaway; };

    protected:
        void reset_timers() {
            _current_state_duration.reset();
            _heating_time_window.reset();
        }

    private:
        const Config _cfg;

        Timer _current_state_duration;
        State _current_state = State::UNDEFINED;
        State _last_state = State::UNDEFINED;

        double _temperature_time_window = 0;
        IntervalTimer _heating_time_window;

        bool _is_runaway = false;
        double _setpoint = 0.0;
    };

public:
    struct Config {
        PID::Config pid_cfg;
        double max_heater_setpoint = 40.0;
        double steady_state_hysteresis = 0.3;
        time_s cooldown_min_length = time_s(180);

        HardwareStack::Idx climate_temperature_idx;
        HardwareStack::Idx heating_surface_temperature_idx;
        HardwareStack::Idx heating_element_idx;

        ThermalRunAway::Config climate_trp_cfg; // run away protection with regards to climate sensor
    };

public:
    Heater(const Config&& cfg, io::HardwareStack& hws)
        : _cfg(cfg), _hws(hws), _pid(std::move(_cfg.pid_cfg)),
          _heating_element(_hws.analogue_actuator(_cfg.heating_element_idx)),
          _surface_temperature(_hws.analog_sensor(_cfg.heating_surface_temperature_idx)),
          _climate_temperature(_hws.analog_sensor(_cfg.climate_temperature_idx)),
          _temperature_source(&_surface_temperature), _update_interval(IntervalTimer(_cfg.pid_cfg.sample_interval)),
          _climate_trp(std::move(_cfg.climate_trp_cfg)) {}

    void initialize() {
        _pid.initialize();

        const auto current_temp = temperature();
        _pid.new_reading(current_temp);

        DBG("Heater initialized: Current surface temperature: %.2f C, initial response : %f", current_temp,
            _pid.response());
    }

    void update() {
        if (_update_interval.expired()) {
            guard_temperature_limits();

            const auto current_temp = temperature();
            _pid.new_reading(current_temp);
            const auto response = _pid.response();
            const auto normalized_response = _pid.setpoint() > 0 ? response / _cfg.pid_cfg.output_upper_limit : 0;
            // DBG("Heater update: current temp: %f, response %f, normalized response %f for sp %f", current_temp,
            // response, normalized_response, _pid.setpoint());
            assert(normalized_response >= 0.0 && normalized_response <= 1.0);
            _heating_element.fade_to(guarded_setpoint(normalized_response));
            update_state();

            _climate_trp.update(state(), _climate_temperature.value());
        }
    }

    void safe_shutdown(bool critical = false) {
        if (critical) {
        }
        _heating_element.fade_to(LogicalState::OFF);
    }

    void set_temperature_source(const TemperatureSource source) {
        switch (source) {
        case TemperatureSource::SURFACE: _temperature_source = &_surface_temperature; break;
        case TemperatureSource::CLIMATE: _temperature_source = &_climate_temperature; break;
        default: assert(!"Unhandled TemperatureSource");
        }
    }

    /// Block until a temperature threshold has been reached
    void block_until_setpoint(const double setpoint, time_ms timeout = time_ms(0), bool saturated = true) {
        auto timer = AlarmTimer(timeout);
        while (temperature() < setpoint && (!timer.expired() || timeout == time_ms(0))) {
            _heating_element.fade_to(guarded_setpoint(LogicalState::ON));
            DBG("Waiting until temperature of %.2fC reaches %.2fC, saturating thermal capacitance (surfaceT %.2f)",
                temperature(), setpoint, _surface_temperature.value());
            _hws.update_all(); // make sure to update sensors
            HAL::delay(time_ms(1000));
        }
        _heating_element.fade_to(LogicalState::OFF);
        if (saturated) return;
        while (temperature() > setpoint && (!timer.expired() || timeout == time_ms(0))) {
            DBG("Waiting until temperature of %f C reaches %f C, unloading thermal capacitance (surfaceT %.2f)",
                temperature(), setpoint, _surface_temperature.value());
            _hws.update_all(); // make sure to update sensors
            HAL::delay(time_ms(1000));
        }
    }

    void autotune(PID::TuneConfig&& cfg, std::function<void()> process_loop = {}) {
        block_until_setpoint(cfg.startpoint);
        set_target_setpoint(cfg.setpoint);
        const auto process_setter = [&](double pwm_value) {
            const auto normalized_response = (pwm_value - _cfg.pid_cfg.output_lower_limit)
                                             / (_cfg.pid_cfg.output_upper_limit - _cfg.pid_cfg.output_lower_limit);
            const auto guarded_normalized_response = guarded_setpoint(normalized_response);
            DBG("Autotune: Setting heating element to output: %.3f (surface: %.3fC, climate %.3fC)",
                guarded_normalized_response, _surface_temperature.value(), _climate_temperature.value());
            _heating_element.fade_to(guarded_normalized_response);
        };
        const auto process_getter = [&]() {
            _hws.update_all(); // make sure to update sensors
            return temperature();
        };
        _pid.autotune(cfg, process_setter, process_getter, process_loop);
        update_state();
    }

    void set_target_setpoint(const Value setpoint) {
        _pid.set_target_setpoint(std::min(_cfg.max_heater_setpoint, setpoint));
        _climate_trp.adjust_setpoint(setpoint);
    }
    Value setpoint() const { return _pid.setpoint(); }

    Value temperature() const {
        assert(_temperature_source != nullptr);
        return _temperature_source->value();
    }
    Value error() const { return std::fabs(temperature() - setpoint()); }

    Value throttle() const { return _heating_element.value(); }

    State state() const { return _state; }

private:
    /// makes sure that when the surface temperature is exceeding the limit, that the power is decreased
    double guarded_setpoint(double setpoint) {
        const auto surface_temperature = _surface_temperature.value();
        auto excess = surface_temperature - _cfg.max_heater_setpoint;
        auto feedback = excess / 10.0;

        const auto adjusted_setpoint = std::clamp(excess > 0.0 ? setpoint - feedback : setpoint, 0.0, 1.0);

        if (adjusted_setpoint != setpoint) {
            DBG("Heater: T %.2f C is above limit of T %.2f C, clamping response from %.2f to %.2f", surface_temperature,
                _cfg.max_heater_setpoint, setpoint, adjusted_setpoint);
        }
        return adjusted_setpoint;
    }

    /// When any temperature readings are out of limits, shutdown the system
    void guard_temperature_limits() {
        //
        if (_surface_temperature.value() < MIN_SURFACE_TEMPERATURE
            || _surface_temperature.value() > MAX_SURFACE_TEMPERATURE) {
            spn::throw_exception(spn::assertion_exception("Heater: Heater element temperature out of limits"));
        }
        if (_climate_temperature.value() < MIN_INSIDE_TEMPERATURE
            || _climate_temperature.value() > MAX_INSIDE_TEMPERATURE) {
            spn::throw_exception(spn::assertion_exception("Heater: Climate temperature out of limits"));
        }

        if (_climate_trp.is_runaway()) {
            HAL::printf("Runaway detected: surfaceT %.2f, climateT %.2f, throttle: %i/255, state: %s",
                        _surface_temperature.value(), _climate_temperature.value(), int(throttle() * 255),
                        std::string(as_stringview(state())).c_str());
            spn::throw_exception(spn::assertion_exception("Heater: Run away detected"));
        }
    }

    /// Update the internal heater state
    void update_state() {
        constexpr auto pid_residu = 5; // consider less than 5/255 controller residu
        const auto is_element_heating = int(throttle() * 255) > pid_residu;

        if (error() < _cfg.steady_state_hysteresis) {
            _state = State::STEADY_STATE;
        } else if (is_element_heating) {
            _state = State::HEATING;
        } else {
            if (_cooled_down_for.timeSinceLast(false) < _cfg.cooldown_min_length) {
                _state = State::COOLING;
            } else {
                _state = State::IDLE;
            }
        }

        // keep track of cooldown time
        if (_state != State::COOLING && _state != State::IDLE) _cooled_down_for.reset();
    }

private:
    const Config _cfg;
    HardwareStack& _hws;

    // double _lowest_reading = 0;
    // double _highest_reading = 0;
    State _state = State::IDLE;

    PID _pid;

    AnalogueActuator _heating_element;

    const AnalogueSensor& _surface_temperature;
    const AnalogueSensor& _climate_temperature;

    const AnalogueSensor* _temperature_source;

    Timer _cooled_down_for = Timer{};

    IntervalTimer _update_interval;

    ThermalRunAway _climate_trp;
};

} // namespace kaskas::io
