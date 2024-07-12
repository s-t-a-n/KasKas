#pragma once

#include "kaskas/component.hpp"
#include "kaskas/events.hpp"
#include "kaskas/io/controllers/heater.hpp"
#include "kaskas/io/peripherals/DS18B20_Temp_Probe.hpp"
#include "kaskas/io/peripherals/DS3231_RTC_EEPROM.hpp"
#include "kaskas/io/peripherals/SHT31_TempHumidityProbe.hpp"
#include "kaskas/io/peripherals/fan.hpp"
#include "kaskas/io/peripherals/relay.hpp"
#include "kaskas/io/providers/clock.hpp"

#include <spine/controller/pid.hpp>
#include <spine/controller/sr_latch.hpp>
#include <spine/core/debugging.hpp>
#include <spine/core/schedule.hpp>
#include <spine/eventsystem/eventsystem.hpp>
#include <spine/filter/implementations/bandpass.hpp>
#include <spine/filter/implementations/ewma.hpp>
#include <spine/io/sensor.hpp>
#include <spine/platform/hal.hpp>

namespace kaskas::component {

using kaskas::Component;
using spn::controller::PID;
using spn::core::Exception;
using spn::core::time::Schedule;
using spn::core::time::Timer;
using Events = kaskas::Events;
using spn::eventsystem::Event;
using spn::eventsystem::EventHandler;
using EventSystem = spn::core::EventSystem;
using spn::core::time::AlarmTimer;

using kaskas::io::DS18B20TempProbe;
using Heater = kaskas::io::Heater;
using kaskas::io::SHT31TempHumidityProbe;
using spn::controller::SRLatch;
using spn::core::time::IntervalTimer;

namespace detail {
inline double inverted(double value, double base = 100.0) { return base - value; }
} // namespace detail

class ClimateControl final : public kaskas::Component {
public:
    struct Config {
        io::HardwareStack::Idx hws_power_idx;
        io::HardwareStack::Idx clock_idx;

        struct Ventilation {
            io::HardwareStack::Idx hws_climate_fan_idx;
            io::HardwareStack::Idx climate_humidity_idx;

            PID::Config climate_fan_pid;
            double minimal_duty_cycle = 0; // value between 0 and 1 where 0.5 means 50% dutycycle
            Schedule::Config schedule_cfg;
            time_s check_interval;
        } ventilation;
        struct Heating {
            io::HardwareStack::Idx heating_element_fan_idx;
            io::HardwareStack::Idx heating_element_temp_sensor_idx;
            io::HardwareStack::Idx climate_temp_sensor_idx;
            io::HardwareStack::Idx ambient_temp_idx;

            Heater::Config heater_cfg;
            Schedule::Config schedule_cfg;
            time_s check_interval;
        } heating;
    };

public:
    ClimateControl(io::HardwareStack& hws, const Config& cfg) : ClimateControl(hws, nullptr, cfg) {}
    ClimateControl(io::HardwareStack& hws, EventSystem* evsys, const Config& cfg)
        : Component(evsys, hws), _cfg(std::move(cfg)), _clock(_hws.clock(_cfg.clock_idx)),
          _ambient_temp_sensor(_hws.analog_sensor(_cfg.heating.ambient_temp_idx)),
          _climate_fan(_hws.analogue_actuator(_cfg.ventilation.hws_climate_fan_idx)),
          _ventilation_control(std::move(_cfg.ventilation.climate_fan_pid)),
          _ventilation_schedule(std::move(_cfg.ventilation.schedule_cfg)),
          _climate_humidity(_hws.analog_sensor(_cfg.ventilation.climate_humidity_idx)),
          _climate_temperature(_hws.analog_sensor(_cfg.heating.climate_temp_sensor_idx)),
          _heating_element_fan(_hws.analogue_actuator(_cfg.heating.heating_element_fan_idx)),
          _heating_element_sensor(_hws.analog_sensor(_cfg.heating.heating_element_temp_sensor_idx)),
          _heater(std::move(cfg.heating.heater_cfg), _hws), _heating_schedule(std::move(_cfg.heating.schedule_cfg)),
          _power(_hws.digital_actuator(_cfg.hws_power_idx)){};

    void initialize() override {
        //
        _heater.set_temperature_source(Heater::TemperatureSource::CLIMATE);
        _heater.initialize();
        _ventilation_control.initialize();

        evsys()->attach(Events::VentilationAutoTune, this);
        evsys()->attach(Events::VentilationCycleCheck, this);
        evsys()->attach(Events::VentilationFollowUp, this);
        evsys()->attach(Events::VentilationCycleStart, this);
        evsys()->attach(Events::VentilationCycleStop, this);
        evsys()->attach(Events::HeatingAutoTune, this);
        evsys()->attach(Events::HeatingFollowUp, this);
        evsys()->attach(Events::HeatingCycleCheck, this);
        evsys()->attach(Events::HeatingCycleStart, this);
        evsys()->attach(Events::HeatingCycleStop, this);

        // starts autotuning now!
        // evsys()->schedule(evsys()->event(Events::HeatingAutoTune, time_s(1)));
        // evsys()->schedule(evsys()->event(Events::VentilationAutoTune, time_s(1)));

        auto time_from_now = time_s(15);
        DBGF("ClimateControl: Scheduling VentilationCycleCheck in %u seconds.", time_from_now.printable());
        evsys()->schedule(evsys()->event(Events::VentilationCycleCheck, time_from_now));
        evsys()->schedule(evsys()->event(Events::VentilationFollowUp, time_from_now));

        time_from_now += time_s(5);
        DBGF("ClimateControl: Scheduling HeatingCycleCheck in %u seconds.", time_from_now.printable());
        evsys()->schedule(evsys()->event(Events::HeatingCycleCheck, time_from_now));
        evsys()->schedule(evsys()->event(Events::HeatingFollowUp, time_from_now));
    }

    void safe_shutdown(State state) override {
        DBGF("ClimateControl: safe shutdown");
        // fade instead of cut to minimalize surges
        HAL::delay_ms(100);
        _climate_fan.fade_to(0.0);
        HAL::delay_ms(100);
        _heater.safe_shutdown(state == State::CRITICAL);
        HAL::delay_ms(100);
        _power.set_state(LogicalState::OFF);
    }

    void handle_event(const Event& event) override {
        const auto now_s = [&]() {
            assert(_clock.is_ready());
            const auto now_dt = _clock.now();
            return time_s(time_h(now_dt.getHour())) + time_m(now_dt.getMinute());
        };
        // const auto scheduled_next_block = [&]() { return; };
        // const auto scheduled_value = [&]() { return _heating_schedule.value_at(now_s()); };

        switch (static_cast<Events>(event.id())) {
        case Events::VentilationFollowUp: {
            // DBG("Ventilation: FollowUp.");
            ventilation_loop();
            evsys()->schedule(evsys()->event(Events::VentilationFollowUp, _cfg.ventilation.check_interval));
            break;
        }
        case Events::VentilationCycleCheck: {
            const auto now = now_s();
            const auto next_setpoint = _ventilation_schedule.value_at(now);

            if (next_setpoint > 0 && _ventilation_control.setpoint() > 0) {
                DBGF("Ventilation: Check: ventilation is currently on, update value to humidity setpoint: %.2f",
                     next_setpoint);
                _ventilation_control.set_target_setpoint(detail::inverted(next_setpoint));
            } else if (next_setpoint > 0 && _ventilation_control.setpoint() == 0) {
                DBGF("Ventilation: Check: ventilation is currently off, turn on and set humidity setpoint to: %.2f",
                     next_setpoint);
                evsys()->trigger(evsys()->event(Events::VentilationCycleStart, time_s(1)));
            } else if (next_setpoint == 0 && _heater.setpoint() > 0) {
                DBGF("Ventilation: Check: ventilation is currently on, turn off");
                evsys()->trigger(evsys()->event(Events::VentilationCycleStop, time_s(1)));
            }

            const auto time_until_next_check = _ventilation_schedule.start_of_next_block(now) - now;
            assert(_ventilation_schedule.start_of_next_block(now) > now);

            DBGF("Ventilation: Checked. Scheduling next check in %u m", time_m(time_until_next_check).printable());
            assert(time_until_next_check.raw<>() > 0);
            evsys()->schedule(evsys()->event(Events::VentilationCycleCheck, time_until_next_check));
            break;
        }
        case Events::VentilationCycleStart: {
            DBG("Ventilation: Start.");
            _power.set_state(LogicalState::ON);

            const auto next_setpoint = _ventilation_schedule.value_at(now_s());

            DBGF("Ventilation: Setting humidity setpoint to %.2f", next_setpoint);
            _ventilation_control.set_target_setpoint(detail::inverted(next_setpoint));
            break;
        }
        case Events::VentilationCycleStop: {
            DBG("Ventilation: Stopped.");
            _climate_fan.creep_to(LogicalState::OFF, _cfg.ventilation.check_interval);
            adjust_power_state();
            break;
        }
        case Events::VentilationAutoTune: {
            DBG("VentilationAutoTune: start.");

            const auto autotune_setpoint = detail::inverted(70.0);

            _power.set_state(LogicalState::ON);
            _climate_fan.fade_to(LogicalState::OFF);

            const auto process_getter = [&]() { return detail::inverted(_climate_humidity.value()); };
            const auto process_setter = [&](double value) {
                _climate_fan.fade_to(value / _cfg.ventilation.climate_fan_pid.output_upper_limit);
            };
            const auto process_loop = [&]() { _hws.update_all(); };

            _ventilation_control.autotune(
                PID::TuneConfig{.setpoint = autotune_setpoint,
                                .startpoint = 0,
                                .hysteresis = 0.3,
                                .satured_at_start = false,
                                .cycles = 30,
                                .aggressiveness = spn::controller::PIDAutotuner::ZNMode::BasicPID},
                process_setter, process_getter, process_loop);
            _climate_fan.fade_to(LogicalState::OFF);
            adjust_power_state();
            break;
        }
        case Events::HeatingAutoTune: {
            DBG("HeatingAutoTune: start.");

            const auto autotune_setpoint = 25.0;
            const auto autotune_startpoint = autotune_setpoint - 0.5;

            _power.set_state(LogicalState::ON);

            // remove residual heat first
            _climate_fan.fade_to(LogicalState::ON);
            for (auto t = _climate_temperature.value(); t > autotune_startpoint; t = _climate_temperature.value()) {
                DBGF("Ventilating for temperature %.2f C is lower than startpoint %.2f C before autotune "
                     "start",
                     t, autotune_startpoint);
                _hws.update_all();
                HAL::delay(time_s(1));
            }
            _climate_fan.fade_to(LogicalState::OFF);

            const auto process_loop = [&]() { _hws.update_all(); };

            _heating_element_fan.fade_to(LogicalState::ON);
            _heater.autotune(PID::TuneConfig{.setpoint = autotune_setpoint,
                                             .startpoint = autotune_startpoint,
                                             .hysteresis = 0.03,
                                             .satured_at_start = true,
                                             .cycles = 10},
                             process_loop);
            adjust_power_state();
            adjust_heater_fan_state();
            break;
        }
        case Events::HeatingFollowUp: {
            // DBG("Heating: FollowUp.");
            heating_control_loop();
            evsys()->schedule(evsys()->event(Events::HeatingFollowUp, _cfg.heating.check_interval));
            break;
        }
        case Events::HeatingCycleCheck: {
            const auto now = now_s();
            const auto next_setpoint = _heating_schedule.value_at(now);

            if (next_setpoint > 0 && _heater.setpoint() > 0) {
                DBGF("Heater: Check: heater is currently on, update value to setpoint: %.2f", next_setpoint);
                _heater.set_target_setpoint(next_setpoint);
            } else if (next_setpoint > 0 && _heater.setpoint() == 0) {
                DBGF("Heater: Check: heater is currently off, turn on and set setpoint to: %.2f", next_setpoint);
                evsys()->trigger(evsys()->event(Events::HeatingCycleStart, time_s(1)));
            } else if (next_setpoint == 0 && _heater.setpoint() > 0) {
                DBGF("Heater: Check: heater is currently on, turn off");
                evsys()->trigger(evsys()->event(Events::HeatingCycleStop, time_s(1)));
            }

            const auto time_until_next_check = _heating_schedule.start_of_next_block(now) - now;
            assert(_heating_schedule.start_of_next_block(now) > now);

            DBGF("Heating: Checked. Scheduling next check in %u m", time_m(time_until_next_check).printable());
            assert(time_until_next_check.raw<>() > 0);
            evsys()->schedule(evsys()->event(Events::HeatingCycleCheck, time_until_next_check));
            break;
        }
        case Events::HeatingCycleStart: {
            // DBG("Heating: Start.");
            _power.set_state(LogicalState::ON);

            const auto next_setpoint = _heating_schedule.value_at(now_s());
            DBGF("Heating: HeatingCycleStart: Setting climate control setpoint to %.2f", next_setpoint);

            _heater.set_target_setpoint(next_setpoint);
            break;
        }
        case Events::HeatingCycleStop: {
            //
            DBG("Heating: Stop.");
            _heater.set_target_setpoint(LogicalState::OFF);
            _heater.safe_shutdown();
            adjust_power_state();
            break;
        }
        default: assert(!"Event was not handled!"); break;
        };
    }

    std::unique_ptr<prompt::RPCRecipe> rpc_recipe() override {
        using namespace prompt;
        auto model = std::make_unique<RPCRecipe>(RPCRecipe(
            "CC", //
            {
                RPCModel("heaterStatus",
                         [this](const OptStringView&) {
                             return RPCResult(std::string(magic_enum::enum_name(_heater.state())));
                         }),
                RPCModel("heaterSetpoint",
                         [this](const OptStringView&) { return RPCResult(std::to_string(_heater.setpoint())); }),
            }));
        return std::move(model);
    }

    void sideload_providers(io::VirtualStackFactory& ssf) override {
        ssf.hotload_provider(DataProviders::HEATING_SETPOINT,
                             std::make_shared<io::ContinuousValue>([this]() { return this->_heater.setpoint(); }));
        ssf.hotload_provider(DataProviders::CLIMATE_HUMIDITY_SETPOINT, std::make_shared<io::ContinuousValue>([this]() {
                                 return 100.0 - this->_ventilation_control.setpoint();
                             }));
    }

private:
    /// continuous checking of heating status
    void heating_control_loop() {
        adjust_heater_fan_state();

        if (_heater.setpoint() == 0.0) {
            return;
        }

        // if (_print_interval.expired()) {
        //     const auto state_str = std::string(Heater::as_stringview(_heater.state())).c_str();
        //     DBGF("Heating: ambientT %.2fC, surfaceT %.2f, climateT %.2fC, sp T %.2f C, throttle: %i/255 state: %s",
        //          _ambient_temp_sensor.value(), _heating_element_sensor.value(), _climate_temperature.value(),
        //          _heater.setpoint(), int(_heater.throttle() * 255), state_str);
        // }

        _heater.update();

        if (_heater.state() != Heater::State::IDLE) {
            _power.set_state(LogicalState::ON);
        }
    }

    /// continuous checking of ventilation status
    void ventilation_loop() {
        const auto climate_humidity = _climate_humidity.value();
        _ventilation_control.new_reading(detail::inverted(climate_humidity));
        const auto normalized_response =
            _ventilation_control.response() / _cfg.ventilation.climate_fan_pid.output_upper_limit;
        // DBGF("Ventilation: current humidity %.2f %%, fan: %.2f%%", climate_humidity, normalized_response * 100);

        if (normalized_response > _cfg.ventilation.minimal_duty_cycle) {
            _climate_fan.creep_to(normalized_response, _cfg.ventilation.check_interval);
        } else {
            _climate_fan.creep_to(LogicalState::OFF, _cfg.ventilation.check_interval);
        }
    }

    /// If no power is needed, turn off the power
    void adjust_power_state() {
        if ((_heater.setpoint() == 0 || _heater.state() == Heater::State::IDLE) && _climate_fan.value() == 0)
            _power.set_state(LogicalState::OFF);
    }

    /// Activate heater fan when heating or when cooling down
    void adjust_heater_fan_state() {
        if (_heater.state() != Heater::State::IDLE) {
            _heating_element_fan.fade_to(LogicalState::ON);
        } else {
            _heating_element_fan.fade_to(LogicalState::OFF);
        }
    }

private:
    const Config _cfg;

    const io::Clock& _clock;

    const io::AnalogueSensor& _ambient_temp_sensor;

    io::AnalogueActuator& _climate_fan;
    PID _ventilation_control;
    Schedule _ventilation_schedule;
    const io::AnalogueSensor& _climate_humidity;
    const io::AnalogueSensor& _climate_temperature;

    Heater _heater;
    io::AnalogueActuator& _heating_element_fan;
    const io::AnalogueSensor& _heating_element_sensor;

    Schedule _heating_schedule;

    io::DigitalActuator& _power;

    IntervalTimer _print_interval = IntervalTimer(time_s(10));
};

} // namespace kaskas::component