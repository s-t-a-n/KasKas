#pragma once

#include "kaskas/io/stack.hpp"

#include <spine/controller/pid.hpp>
#include <spine/core/debugging.hpp>
#include <spine/filter/implementations/bandpass.hpp>
#include <spine/filter/implementations/ewma.hpp>
#include <spine/io/sensor.hpp>
#include <spine/platform/hal.hpp>

#include <set>

namespace kaskas::io {

using spn::controller::PID;
using spn::core::time::AlarmTimer;
using spn::core::time::IntervalTimer;
using spn::filter::EWMA;

// todo: put this in a configuration file

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
    enum class State { IDLE, COOLING_DOWN, HEATING_UP, STEADY_STATE, THERMAL_RUN_AWAY };

    using Value = double;

    struct Config {
        PID::Config pid_cfg;
        double max_heater_setpoint = 60.0;

        HardwareStack::Idx temperature_sensor_idx;
        HardwareStack::Idx heating_element_idx;
    };

public:
    Heater(const Config&& cfg, io::HardwareStack& hws)
        : _cfg(cfg), _hws(hws), _pid(std::move(_cfg.pid_cfg)),
          _heating_element(_hws.analogue_actuator(_cfg.heating_element_idx)),
          _temperature(_hws.analog_sensor(_cfg.temperature_sensor_idx)),
          _update_interval(IntervalTimer(_cfg.pid_cfg.sample_interval)) {}

    void initialize() {
        _pid.initialize();

        const auto current_temp = _temperature.value();
        _pid.new_reading(current_temp);

        _lowest_reading = current_temp;
        _highest_reading = current_temp;

        DBGF("Heater initialized: Current surface temperature: %.2f C, initial response : %f", current_temp,
             _pid.response());
    }

    void update() {
        if (_update_interval.expired()) {
            const auto current_temp = _temperature.value();
            _pid.new_reading(current_temp);
            const auto response = _pid.response();
            const auto normalized_response = _pid.setpoint() > 0 ? response / _cfg.pid_cfg.output_upper_limit : 0;
            // DBGF("Heater update: current temp: %f, response %f, normalized response %f for sp %f", current_temp,
            // response, normalized_response, _pid.setpoint());
            assert(normalized_response >= 0.0 && normalized_response <= 1.0);
            _heating_element.fade_to(normalized_response);
            update_state();
        }
    }

    ///
    // void block_until_setpoint(const double setpoint, time_ms timeout = time_ms(0), bool saturated = false) {
    //     auto timer = AlarmTimer(timeout);
    //     while (_temperature.read() < setpoint && (!timer.expired() || timeout == time_ms(0))) {
    //         _heating_element.set_value(1.0);
    //         DBGF("Waiting until temperature of %f C reaches %f C, saturating thermal capacitance",
    //         _temperature.value(),
    //              setpoint);
    //         HAL::delay(time_ms(1000));
    //     }
    //     _heating_element.fade_to(0.0);
    //     if (saturated)
    //         return;
    //     while (_temperature.read() > setpoint && (!timer.expired() || timeout == time_ms(0))) {
    //         DBGF("Waiting until temperature of %f C reaches %f C, unloading thermal capacitance",
    //         _temperature.value(),
    //              setpoint);
    //         HAL::delay(time_ms(1000));
    //     }
    // }

    void autotune(PID::TuneConfig&& cfg) {
        // block_until_setpoint(startpoint);
        // set_setpoint(setpoint);
        const auto process_setter = [&](double pwm_value) {
            //
            const auto normalized_response = (pwm_value - _cfg.pid_cfg.output_lower_limit)
                                             / (_cfg.pid_cfg.output_upper_limit - _cfg.pid_cfg.output_lower_limit);
            DBGF("Setting heating element to output: %f", normalized_response);
            _heating_element.fade_to(normalized_response);
        };
        const auto process_getter = [&]() {
            update();
            return temperature();
        };
        _pid.autotune(cfg, process_setter, process_getter);
    }

    void set_setpoint(const Value setpoint) { _pid.set_target_setpoint(std::min(_cfg.max_heater_setpoint, setpoint)); }
    Value setpoint() const { return _pid.setpoint(); }

    Value temperature() const { return _temperature.value(); }
    Value error() const { return std::fabs(temperature() - setpoint()); }

    State state() const { return _state; }

    void safe_shutdown(bool critical = false) {
        if (critical) {
        }
        _heating_element.fade_to(0);
    }

    // Config& cfg() const { return _cfg; }

private:
    void update_state() {
        const auto current_temp = temperature();
        if (current_temp < _lowest_reading)
            _lowest_reading = current_temp;
        if (current_temp > _highest_reading)
            _highest_reading = current_temp;

        const auto distance_to_lowest = std::fabs(_lowest_reading - current_temp);
        const auto distance_to_highest = std::fabs(_highest_reading - current_temp);

        if (setpoint() == 0 && distance_to_lowest < distance_to_highest) {
            _state = State::IDLE;
        } else if (setpoint() == 0 && distance_to_highest < distance_to_lowest) {
            _state = State::COOLING_DOWN;
        } else if (setpoint() > 0 && error() < TRP_STABLE_HYSTERESIS_C) {
            _state = State::STEADY_STATE;
        } else {
            _state = State::HEATING_UP;
        }
        // DBGF("current: %f, distance_to_lowest : %f, distance_to_highest: %f, setpoint: %f, error: %f ", current_temp,
        // distance_to_lowest, distance_to_highest, setpoint(), error());

        // if (state() == State::IDLE)
        //     DBG("IDLE");
        // if (state() == State::COOLING_DOWN)
        //     DBG("COOLING_DOWN");
        // if (state() == State::STEADY_STATE)
        //     DBG("STEADY_STATE");
        // if (state() == State::HEATING_UP)
        //     DBG("HEATING_UP");
    }

private:
    const Config _cfg;
    HardwareStack& _hws;

    double _lowest_reading = 0;
    double _highest_reading = 0;
    State _state = State::IDLE;

    PID _pid;

    AnalogueActuator _heating_element;
    AnalogueSensor _temperature;

    IntervalTimer _update_interval;
};

} // namespace kaskas::io