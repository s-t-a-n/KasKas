#pragma once

#include "kaskas/io/stack.hpp"

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
using spn::filter::EWMA;

// todo: put this in a configuration file

constexpr double MIN_OUTSIDE_TEMPERATURE = 5.0;
constexpr double MAX_OUTSIDE_TEMPERATURE = 45.0;

constexpr double MIN_INSIDE_TEMPERATURE = 8.0;
constexpr double MAX_INSIDE_TEMPERATURE = 40.0;

constexpr double MIN_SURFACE_TEMPERATURE = 8.0;
constexpr double MAX_SURFACE_TEMPERATURE = 65.0;

// maximal error from setpoint in degrees that is allowed to happen for timewindow TRP_STABLE_TIMEWINDOW until thermal
// runaway is considered
constexpr double TRP_STABLE_HYSTERESIS_C = 4.0;
constexpr int TRP_STABLE_TIMEWINDOW = 40;

// minimal change in degrees within timewindow TRP_CHANGING_TIMEWINDOW
constexpr double TRP_CHANGING_MINIMAL_DELTA_C = 4.0;
constexpr double TRP_CHANGING_TIMEWINDOW = 40;

class ThermalRunAway {
public:
    double* _setpoint;
};

class Heater {
public:
    enum class State { IDLE, COOLING_DOWN, HEATING_UP, STEADY_STATE, OVERHEATED, THERMAL_RUN_AWAY };
    enum class TemperatureSource { SURFACE, CLIMATE };
    using Value = double;

    struct Config {
        PID::Config pid_cfg;
        double max_heater_setpoint = 60.0;
        time_s cooldown_min_length = time_s(180);

        HardwareStack::Idx climate_temperature_idx;
        HardwareStack::Idx heating_surface_temperature_idx;
        HardwareStack::Idx heating_element_idx;
    };

public:
    Heater(const Config&& cfg, io::HardwareStack& hws)
        : _cfg(cfg), _hws(hws), _pid(std::move(_cfg.pid_cfg)),
          _heating_element(_hws.analogue_actuator(_cfg.heating_element_idx)),
          _surface_temperature(_hws.analog_sensor(_cfg.heating_surface_temperature_idx)),
          _climate_temperature(_hws.analog_sensor(_cfg.climate_temperature_idx)),
          _temperature_source(&_surface_temperature), _update_interval(IntervalTimer(_cfg.pid_cfg.sample_interval)) {}

    void initialize() {
        _pid.initialize();

        const auto current_temp = temperature();
        _pid.new_reading(current_temp);

        reset_reading_window_to(current_temp);

        DBGF("Heater initialized: Current surface temperature: %.2f C, initial response : %f", current_temp,
             _pid.response());
    }

    void update() {
        if (_update_interval.expired()) {
            guard_temperature_limits();

            const auto current_temp = temperature();
            _pid.new_reading(current_temp);
            const auto response = _pid.response();
            const auto normalized_response = _pid.setpoint() > 0 ? response / _cfg.pid_cfg.output_upper_limit : 0;
            // DBGF("Heater update: current temp: %f, response %f, normalized response %f for sp %f", current_temp,
            // response, normalized_response, _pid.setpoint());
            assert(normalized_response >= 0.0 && normalized_response <= 1.0);
            _heating_element.fade_to(guarded_setpoint(normalized_response));
            update_state();
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
        reset_reading_window_to(temperature());
    }

    ///
    void block_until_setpoint(const double setpoint, time_ms timeout = time_ms(0), bool saturated = true) {
        auto timer = AlarmTimer(timeout);
        while (temperature() < setpoint && (!timer.expired() || timeout == time_ms(0))) {
            _heating_element.fade_to(guarded_setpoint(LogicalState::ON));
            DBGF("Waiting until temperature of %.2fC reaches %.2fC, saturating thermal capacitance (surfaceT %.2f)",
                 temperature(), setpoint, _surface_temperature.value());
            _hws.update_all(); // make sure to update sensors
            HAL::delay(time_ms(1000));
        }
        _heating_element.fade_to(LogicalState::OFF);
        if (saturated)
            return;
        while (temperature() > setpoint && (!timer.expired() || timeout == time_ms(0))) {
            DBGF("Waiting until temperature of %f C reaches %f C, unloading thermal capacitance (surfaceT %.2f)",
                 temperature(), setpoint, _surface_temperature.value());
            _hws.update_all(); // make sure to update sensors
            HAL::delay(time_ms(1000));
        }
    }

    void autotune(PID::TuneConfig&& cfg, std::function<void()> process_loop = {}) {
        block_until_setpoint(cfg.startpoint);
        set_setpoint(cfg.setpoint);
        const auto process_setter = [&](double pwm_value) {
            const auto normalized_response = (pwm_value - _cfg.pid_cfg.output_lower_limit)
                                             / (_cfg.pid_cfg.output_upper_limit - _cfg.pid_cfg.output_lower_limit);
            const auto guarded_normalized_response = guarded_setpoint(normalized_response);
            DBGF("Autotune: Setting heating element to output: %.3f (surface: %.3fC, climate %.3fC)",
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

    void set_setpoint(const Value setpoint) { _pid.set_target_setpoint(std::min(_cfg.max_heater_setpoint, setpoint)); }
    Value setpoint() const { return _pid.setpoint(); }

    Value temperature() const {
        assert(_temperature_source != nullptr);
        return _temperature_source->value();
    }
    Value error() const { return std::fabs(temperature() - setpoint()); }

    State state() const { return _state; }

    // Config& cfg() const { return _cfg; }

private:
    /// makes sure that when the surface temperature is exceeding the limit, that the power is decreased
    double guarded_setpoint(double setpoint) {
        const auto surface_temperature = _surface_temperature.value();
        auto excess = surface_temperature - _cfg.max_heater_setpoint;
        auto feedback = excess / 10.0;

        const auto adjusted_setpoint = std::clamp(excess > 0.0 ? setpoint - feedback : setpoint, 0.0, 1.0);

        if (adjusted_setpoint != setpoint) {
            DBGF("Heater: T %.2f C is above limit of T %.2f C, clamping response from %.2f to %.2f",
                 surface_temperature, _cfg.max_heater_setpoint, setpoint, adjusted_setpoint);
        }
        return adjusted_setpoint;
    }

    /// When any temperature readings are out of limits, shutdown the system
    void guard_temperature_limits() {
        //
        if (_surface_temperature.value() < MIN_SURFACE_TEMPERATURE
            || _surface_temperature.value() > MAX_SURFACE_TEMPERATURE) {
            dbg::throw_exception(
                spn::core::assertion_error("ClimateCountrol: Heater element temperature out of limits"));
        }
        if (_climate_temperature.value() < MIN_INSIDE_TEMPERATURE
            || _climate_temperature.value() > MAX_INSIDE_TEMPERATURE) {
            dbg::throw_exception(spn::core::assertion_error("ClimateCountrol: Climate temperature out of limits"));
        }
    }

    void reset_reading_window_to(const double value = 0.0) {
        _lowest_reading = value;
        _highest_reading = value;
    }

    void update_state() {
        const auto current_temp = _surface_temperature.value();
        if (current_temp < _lowest_reading)
            _lowest_reading = current_temp;
        if (current_temp > _highest_reading)
            _highest_reading = current_temp;

        const auto distance_to_lowest = std::fabs(_lowest_reading - current_temp);
        const auto distance_to_highest = std::fabs(_highest_reading - current_temp);
        // DBGF("distances to lowest: %f, distance to highest: %f", distance_to_lowest, distance_to_highest);

        if (current_temp > _cfg.max_heater_setpoint) {
            _state = State::OVERHEATED;
        } else if (setpoint() > 0 && temperature() < setpoint()) {
            _state = State::HEATING_UP;
        } else if (setpoint() > 0 && error() < TRP_STABLE_HYSTERESIS_C) {
            _state = State::STEADY_STATE;
        } else if (_cooled_down_for.timeSinceLast(false) > _cfg.cooldown_min_length) {
            _state = State::IDLE;
        } else if (setpoint() == 0 && distance_to_highest < distance_to_lowest) {
            _state = State::COOLING_DOWN;
        } else if (setpoint() > 0 && temperature() > setpoint()) {
            _state = State::COOLING_DOWN;
        } else {
            assert(!"Unhandled case");
        }
        // DBGF("current: %f, distance_to_lowest : %f, distance_to_highest: %f, setpoint: %f, error: %f ",
        // current_temp, distance_to_lowest, distance_to_highest, setpoint(), error());

        // keep track of cooldown time
        if (_state != State::COOLING_DOWN && _state != State::IDLE)
            _cooled_down_for.reset();

        // if (state() == State::IDLE)
        // DBG("IDLE");
        // if (state() == State::COOLING_DOWN)
        // DBG("COOLING_DOWN");
        if (state() == State::STEADY_STATE)
            DBG("STEADY_STATE");
        if (state() == State::HEATING_UP)
            DBG("HEATING_UP");
        if (state() == State::OVERHEATED)
            DBG("OVERHEATED");
    }

private:
    const Config _cfg;
    HardwareStack& _hws;

    double _lowest_reading = 0;
    double _highest_reading = 0;
    State _state = State::IDLE;

    PID _pid;

    AnalogueActuator _heating_element;

    const AnalogueSensor& _surface_temperature;
    const AnalogueSensor& _climate_temperature;

    const AnalogueSensor* _temperature_source;

    Timer _cooled_down_for = Timer{};

    IntervalTimer _update_interval;
};

} // namespace kaskas::io