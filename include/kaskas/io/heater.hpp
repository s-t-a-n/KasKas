#pragma once

#include <spine/controller/pid.hpp>
#include <spine/core/debugging.hpp>
#include <spine/filter/implementations/bandpass.hpp>
#include <spine/filter/implementations/ewma.hpp>
#include <spine/io/sensor.hpp>
#include <spine/platform/hal.hpp>

#include <set>

namespace kaskas::io {

using spn::controller::Pid;
using spn::filter::EWMA;

template<typename TempProbeType>
class Heater {
public:
    enum class State { IDLE, HEATING_UP, STEADY_STATE };

    using Value = double;
    using BandPass = spn::filter::BandPass<double>;
    using TemperatureSensor = Sensor<TempProbeType, BandPass>;

    struct Config {
        Pid::Config pid_cfg;
        AnalogueOutput::Config heater_cfg;

        typename TempProbeType::Config tempprobe_cfg;
        BandPass::Config tempprobe_filter_cfg = // example medium band pass to reject significant outliers
            BandPass::Config{.mode = BandPass::Mode::RELATIVE, .mantissa = 1, .decades = 0.01, .offset = 0};
    };

public:
    Heater(const Config&& cfg)
        : _cfg(cfg), _pid(std::move(_cfg.pid_cfg)), _heater(std::move(_cfg.heater_cfg)),
          _temperature(typename TemperatureSensor::Config{.sensor_cfg = _cfg.tempprobe_cfg,
                                                          .filter_cfg = _cfg.tempprobe_filter_cfg}) {}

    void initialize() {
        _pid.initialize();
        _heater.initialize();
        _temperature.initialize();
        _pid.new_reading(_temperature.read());
    }

    void update() {
        _pid.new_reading(_temperature.read());
        assert(_pid.response() >= 0 && _pid.response() <= 1.0);
        _heater.set_value(_pid.response() / _cfg.pid_cfg.output_upper_limit);

        if (error() < 0.1) {
            _state = State::STEADY_STATE;
        } else if (_heater.value() == 0) {
            _state = State::IDLE;
        } else {
            _state = State::HEATING_UP;
        }
    }

    ///
    void block_until_setpoint(const double setpoint, time_ms timeout = time_ms(0)) {
        auto timer = AlarmTimer(HAL::millis() + timeout);
        while (_temperature.read() < setpoint && (!timer.expired()) || timeout == time_ms(0)) {
            _heater.fade_to(1.0);
            DBGF("Waiting until temperature of %f C reaches %f C, saturating thermal capacitance", _temperature.value(),
                 setpoint);
            HAL::delay(time_ms(1000));
        }
        _heater.fade_to(0.0);
        while (_temperature.read() > setpoint && (!timer.expired()) || timeout == time_ms(0)) {
            DBGF("Waiting until temperature of %f C reaches %f C, unloading thermal capacitance", _temperature.value(),
                 setpoint);
            HAL::delay(time_ms(1000));
        }
    }

    void set_setpoint(const Value setpoint) { _pid.set_target_setpoint(setpoint); }
    Value setpoint() const { return _pid.setpoint(); }

    Value temperature() const { return _temperature.value(); }
    Value error() const { return std::fabs(temperature() - setpoint()); }

    State state() const { return _state; }

private:
    const Config _cfg;

    State _state;

    Pid _pid;
    AnalogueOutput _heater;
    TemperatureSensor _temperature;
};

} // namespace kaskas::io