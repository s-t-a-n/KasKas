#pragma once

#include "kaskas/io/hardware_stack.hpp"

#include <magic_enum/magic_enum.hpp>
#include <spine/controller/pid.hpp>
#include <spine/core/debugging.hpp>
#include <spine/core/logging.hpp>
#include <spine/core/types.hpp>
#include <spine/filter/implementations/ewma.hpp>
#include <spine/platform/hal.hpp>
#include <spine/structure/time/timers.hpp>

namespace kaskas::io {

// todo: put this in a configuration file
constexpr float MIN_INSIDE_TEMPERATURE = 12.0;
constexpr float MAX_INSIDE_TEMPERATURE = 40.0;

constexpr float MIN_SURFACE_TEMPERATURE = 12.0;
constexpr float MAX_SURFACE_TEMPERATURE = 50.0;

class Heater {
public:
    enum class State { UNDEFINED, IDLE, COOLING, HEATING, STEADY_STATE, THERMAL_RUN_AWAY };
    static constexpr std::string_view as_stringview(State state) noexcept { return magic_enum::enum_name(state); }
    enum class TemperatureSource { UNDEFINED, SURFACE, CLIMATE };

    using Value = float;

    // todo: move this into a generic Spine class that monitors using an expected envelope
    class ThermalRunAway {
    public:
        // todo: make an autotuner to find limits of normal usage experimentally
        struct Config {
            k_time_s stable_timewindow = k_time_m(10);

            // minimal change in degrees within timewindow changing_timewindow
            float heating_minimal_rising_c = 0.1;
            float heating_minimal_dropping_c = 0.01;
            k_time_s heating_timewindow = k_time_m(20);

            bool guard_stable_state = true;
            bool guard_changing_state = true;
        };

        ThermalRunAway(const Config&& cfg)
            : _cfg(std::move(cfg)), _current_state_duration(Timer{}), _heating_time_window(_cfg.heating_timewindow) {}

        void update(State state, float temperature) {
            if (state != _current_state) { // heater changed states
                _last_state = _current_state;
                _current_state = state;
                reset_timers();
                _temperature_time_window = temperature;
                return;
            }

            if (_current_state == State::HEATING) {
                if (_cfg.guard_stable_state and _last_state == State::STEADY_STATE) { // heater came out of steady state
                    if (k_time_s(_current_state_duration.time_since_last(false))
                        > _cfg.stable_timewindow) { // heater went outside of steady state for too long
                        WARN("Heater went outside of steady state for too long, triggering run away! (time expired: "
                             "%is, timewindow: %lis, current temperature %.2fC)",
                             k_time_s(_current_state_duration.time_since_last(false)).raw(),
                             k_time_s(_cfg.stable_timewindow).raw(), temperature)
                        _is_runaway = true;
                    }
                    return;
                }

                if (_cfg.guard_changing_state and _heating_time_window.expired()) {
                    if (temperature < _setpoint) { // heater is trying to rise temperature
                        const auto error = temperature - _temperature_time_window;
                        if (error < _cfg.heating_minimal_rising_c) { // temperature didnt rise fast enough
                            WARN("Temperature didnt rise fast enough, triggering run away! (delta:%.2fC, temperature: "
                                 "%.2fC)",
                                 error, temperature);
                            _is_runaway = true;
                        }
                        _temperature_time_window = temperature;
                    }

                    if (temperature > _setpoint) { // heater should be dropping temperature
                        const auto error = _temperature_time_window - temperature;
                        if (error < _cfg.heating_minimal_dropping_c) { // temperature didn't drop fast enough
                            WARN("Temperature didnt drop fast enough, triggering run away! (delta:%.2fC, temperature: "
                                 "%.2fC)",
                                 error, temperature);
                            _is_runaway = true;
                        }
                        _temperature_time_window = temperature;
                    }
                }
            }
        }

        void adjust_setpoint(float setpoint) {
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
        using IntervalTimer = spn::structure::time::IntervalTimer;
        using Timer = spn::structure::time::Timer;

        const Config _cfg;

        Timer _current_state_duration;
        State _current_state = State::UNDEFINED;
        State _last_state = State::UNDEFINED;

        float _temperature_time_window = 0;
        IntervalTimer _heating_time_window;

        bool _is_runaway = false;
        float _setpoint = 0.0;
    };

public:
    using PID = spn::controller::PID;

    struct Config {
        PID::Config pid_cfg;
        float max_heater_setpoint = 40.0;
        float steady_state_hysteresis = 0.3;
        k_time_s cooldown_min_length = k_time_s(180);

        float dynamic_gain_factor = 0; // for every degree of error from sp, adds `dynamic gain` to Kp
        k_time_s dynamic_gain_interval = k_time_s(60); // update gain every n seconds

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
          _dynamic_gain_interval(IntervalTimer(_cfg.dynamic_gain_interval)),
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

            _pid.new_reading(temperature());
            const auto response = _pid.response();
            spn_assert(_cfg.pid_cfg.output_upper_limit > 0);
            auto normalized_response = _pid.setpoint() > 0 ? response / _cfg.pid_cfg.output_upper_limit : 0;
            spn_expect(normalized_response >= 0.0f && normalized_response <= 1.0f);
            normalized_response = std::clamp(normalized_response, 0.0f, 1.0f);
            _heating_element.fade_to(guarded_setpoint(normalized_response));

            update_state();

            _climate_trp.update(state(), _climate_temperature.value());

            if (_cfg.dynamic_gain_factor > 0 && _dynamic_gain_interval.expired()) {
                auto tunings = _pid.tunings();
                auto proportionality = PID::Proportionality::ON_MEASUREMENT;
                if (state() == State::HEATING && temperature() < setpoint()) {
                    tunings.Kp = _cfg.pid_cfg.tunings.Kp * (1 + _cfg.dynamic_gain_factor * error());
                    tunings.Ki = _cfg.pid_cfg.tunings.Ki / (1 + _cfg.dynamic_gain_factor * error());
                    proportionality = PID::Proportionality::ON_ERROR;
                } else {
                    if (tunings == _cfg.pid_cfg.tunings) return;
                    tunings = _cfg.pid_cfg.tunings;
                }
                DBG("Heater: setting new tunings: {%.2f:%.2f:%.2f} over original {%.2f:%.2f:%.2f}", tunings.Kp,
                    tunings.Ki, tunings.Kd, _cfg.pid_cfg.tunings.Kp, _cfg.pid_cfg.tunings.Ki, _cfg.pid_cfg.tunings.Kd);
                _pid.set_tunings(tunings, proportionality);
            }
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
        default: spn_assert(!"Unhandled TemperatureSource");
        }
    }

    /// Block until a temperature threshold has been reached
    void block_until_setpoint(const float setpoint, k_time_ms timeout = k_time_ms(0), bool saturated = true) {
        auto timer = AlarmTimer(timeout);
        while (temperature() < setpoint && (!timer.expired() || timeout == k_time_ms(0))) {
            _heating_element.fade_to(guarded_setpoint(LogicalState::ON));
            DBG("Waiting until temperature of %.2fC reaches %.2fC, saturating thermal capacitance (surfaceT %.2f)",
                temperature(), setpoint, _surface_temperature.value());
            _hws.update_all(); // make sure to update sensors
            HAL::delay(k_time_ms(1000));
        }
        _heating_element.fade_to(LogicalState::OFF);
        if (saturated) return;
        while (temperature() > setpoint && (!timer.expired() || timeout == k_time_ms(0))) {
            DBG("Waiting until temperature of %f C reaches %f C, unloading thermal capacitance (surfaceT %.2f)",
                temperature(), setpoint, _surface_temperature.value());
            _hws.update_all(); // make sure to update sensors
            HAL::delay(k_time_ms(1000));
        }
    }

    PID::Tunings autotune(PID::TuneConfig&& cfg, std::function<void()> process_loop = {}) {
        block_until_setpoint(cfg.startpoint);
        set_target_setpoint(cfg.setpoint);
        const auto process_setter = [&](float pwm_value) {
            spn_assert(_cfg.pid_cfg.output_upper_limit - _cfg.pid_cfg.output_lower_limit > 0);
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
        const auto tunings = _pid.autotune(cfg, process_setter, process_getter, process_loop);
        update_state();
        _pid.set_tunings(tunings);
        LOG("Heater: Autotuning complete, results: kp: %f, ki: %f, kd: %f", tunings.Kp, tunings.Ki, tunings.Kd);
        return tunings;
    }

    void set_tunings(const PID::Tunings& tunings) { _pid.set_tunings(tunings); }
    PID::Tunings tunings() const { return _pid.tunings(); }

    void set_target_setpoint(const Value setpoint) {
        auto clamped_setpoint = std::min(_cfg.max_heater_setpoint, setpoint);
        _pid.set_target_setpoint(clamped_setpoint);
        _climate_trp.adjust_setpoint(clamped_setpoint);
    }
    Value setpoint() const { return _pid.setpoint(); }

    Value temperature() const {
        spn_assert(_temperature_source != nullptr);
        return _temperature_source->value();
    }
    Value error() const { return std::fabs(temperature() - setpoint()); }

    Value throttle() const { return _heating_element.value(); }

    State state() const { return _state; }

private:
    using LogicalState = spn::core::LogicalState;

    /// makes sure that when the surface temperature is exceeding the limit, that the power is decreased
    float guarded_setpoint(float setpoint) {
        const auto surface_temperature = _surface_temperature.value();
        auto excess = surface_temperature - _cfg.max_heater_setpoint;
        auto feedback =
            excess / 10.0f; // clamp the controller to an absolute maximum of 10 degrees above the max heater setpoint

        const auto adjusted_setpoint = std::clamp(excess > 0.0f ? setpoint - feedback : setpoint, 0.0f, 1.0f);

        if (adjusted_setpoint != setpoint) {
            WARN("Heater: T %.2f C is above limit of T %.2f C, clamping response from %.2f to %.2f",
                 surface_temperature, _cfg.max_heater_setpoint, setpoint, adjusted_setpoint);
        }
        return adjusted_setpoint;
    }

    /// When any temperature readings are out of limits, shutdown the system
    void guard_temperature_limits() {
        if (_surface_temperature.value() < MIN_SURFACE_TEMPERATURE
            || _surface_temperature.value() > MAX_SURFACE_TEMPERATURE) {
            spn::throw_exception(spn::assertion_exception("Heater: Heater element temperature out of limits"));
        }
        if (_climate_temperature.value() < MIN_INSIDE_TEMPERATURE
            || _climate_temperature.value() > MAX_INSIDE_TEMPERATURE) {
            spn::throw_exception(spn::assertion_exception("Heater: Climate temperature out of limits"));
        }

        if (_climate_trp.is_runaway()) {
            ERR("Runaway detected: surfaceT %.2f, climateT %.2f, throttle: %i/255, state: %s",
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
            if (_cooled_down_for.time_since_last(false) < _cfg.cooldown_min_length) {
                _state = State::COOLING;
            } else {
                _state = State::IDLE;
            }
        }

        // keep track of cooldown time
        if (_state != State::COOLING && _state != State::IDLE) _cooled_down_for.reset();
    }

private:
    using AlarmTimer = spn::structure::time::AlarmTimer;
    using IntervalTimer = spn::structure::time::IntervalTimer;
    using Timer = spn::structure::time::Timer;

    const Config _cfg;
    HardwareStack& _hws;

    State _state = State::IDLE;

    PID _pid;

    AnalogueActuator _heating_element;

    const AnalogueSensor& _surface_temperature;
    const AnalogueSensor& _climate_temperature;

    const AnalogueSensor* _temperature_source;

    Timer _cooled_down_for = Timer{};

    IntervalTimer _update_interval;
    IntervalTimer _dynamic_gain_interval;

    ThermalRunAway _climate_trp;
};

} // namespace kaskas::io
