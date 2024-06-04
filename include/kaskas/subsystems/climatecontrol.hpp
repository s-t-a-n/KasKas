#pragma once

#include "../../../../../.platformio/packages/toolchain-gccarmnoneeabi/arm-none-eabi/include/sys/_intsup.h"
#include "kaskas/component.hpp"
#include "kaskas/events.hpp"
#include "kaskas/io/clock.hpp"
#include "kaskas/io/heater.hpp"
#include "kaskas/io/implementations/DS18B20_Temp_Probe.hpp"
#include "kaskas/io/implementations/DS3231_RTC_EEPROM.hpp"
#include "kaskas/io/implementations/SHT31_TempHumidityProbe.hpp"
#include "kaskas/io/relay.hpp"

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
using Heater = kaskas::io::Heater<DS18B20TempProbe>;
using kaskas::io::SHT31TempHumidityProbe;
using spn::controller::SRLatch;
using spn::core::time::IntervalTimer;

// todo: put this in a configuration file
constexpr double MIN_OUTSIDE_TEMPERATURE = 5.0;
constexpr double MAX_OUTSIDE_TEMPERATURE = 45.0;

constexpr double MIN_INSIDE_TEMPERATURE = 8.0;
constexpr double MAX_INSIDE_TEMPERATURE = 40.0;

constexpr double MIN_SURFACE_TEMPERATURE = 8.0;
constexpr double MAX_SURFACE_TEMPERATURE = 65.0;

class ClimateControl final : public kaskas::Component {
public:
    struct Config {
        Relay::Config power_cfg;

        struct Ventilation {
            AnalogueOutput::Config fan_cfg;
            time_s single_shot_length; // duration that ventilation will be on at minimum
            double low_humidity;
            double high_humidity;
            time_s minimal_interval; // minimal interval between consecutive shots
            time_s maximal_interval; // maximal length of time that no ventilation takes place
        } ventilation;
        struct Heating {
            AnalogueOutput::Config fan_cfg;
            Heater::Config heater_cfg;
            Schedule::Config schedule_cfg;
            PID::Config climate_control;
            time_s check_interval;
        } heating;
    };

public:
    explicit ClimateControl(const Config& cfg) : ClimateControl(nullptr, cfg) {}
    ClimateControl(EventSystem* evsys, const Config& cfg)
        : Component(evsys), _cfg(std::move(cfg)), _fan(std::move(cfg.ventilation.fan_cfg)),
          _heater_fan(std::move(cfg.heating.fan_cfg)), _heater(std::move(cfg.heating.heater_cfg)),
          _heating_schedule(std::move(_cfg.heating.schedule_cfg)),
          _temperature_humidity_probe(SHT31TempHumidityProbe::Config{}),
          _climate_control(std::move(_cfg.heating.climate_control)),
          _ventilation_control(SRLatch::Config{.high = _cfg.ventilation.high_humidity,
                                               .low = _cfg.ventilation.low_humidity,
                                               .minimal_on_time = _cfg.ventilation.single_shot_length,
                                               .maximal_on_time = _cfg.ventilation.single_shot_length,
                                               .minimal_off_time = _cfg.ventilation.minimal_interval,
                                               .maximal_off_time = _cfg.ventilation.maximal_interval}),
          _power(std::move(cfg.power_cfg)){};

    void initialize() override {
        //
        _fan.initialize();
        _heater_fan.initialize();
        _heater.initialize();
        _power.initialize();
        _climate_control.initialize();
        _ventilation_control.initialize();
        _temperature_humidity_probe.initialize();

        evsys()->attach(Events::VentilationFollowUp, this);
        evsys()->attach(Events::VentilationStart, this);
        evsys()->attach(Events::VentilationStop, this);
        evsys()->attach(Events::HeatingAutoTuneHeater, this);
        evsys()->attach(Events::HeatingAutoTuneClimateControl, this);
        evsys()->attach(Events::HeatingFollowUp, this);
        evsys()->attach(Events::HeatingCycleCheck, this);
        evsys()->attach(Events::HeatingCycleStart, this);
        evsys()->attach(Events::HeatingCycleStop, this);

        // starts autotuning now!
        // evsys()->schedule(evsys()->event(Events::HeatingAutoTuneClimateControl, time_s(1), Event::Data()));

        auto time_from_now = time_s(20);
        DBGF("ClimateControl: Scheduling VentilationCheck in %u seconds.", time_from_now.printable());
        evsys()->schedule(evsys()->event(Events::VentilationFollowUp, time_from_now, Event::Data()));

        time_from_now += time_s(5);
        DBGF("ClimateControl: Scheduling HeatingCheck in %u seconds.", time_from_now.printable());
        evsys()->schedule(evsys()->event(Events::HeatingCycleCheck, time_from_now, Event::Data()));
        evsys()->schedule(evsys()->event(Events::HeatingFollowUp, time_from_now, Event::Data()));
    }

    void safe_shutdown(State state) override {
        DBGF("ClimateControl: safe shutdown");
        // fade instead of cut to minimalize surges
        HAL::delay_ms(100);
        _fan.fade_to(0.0);
        HAL::delay_ms(100);
        _heater.safe_shutdown(state == State::CRITICAL);
        HAL::delay_ms(100);
        _power.set_state(LogicalState::OFF);
    }

    void handle_event(const Event& event) override {
        const auto now_s = [&]() {
            assert(Clock::is_ready());
            const auto now_dt = Clock::now();
            return time_s(time_h(now_dt.getHour())) + time_m(now_dt.getMinute());
        };
        // const auto scheduled_next_block = [&]() { return; };
        // const auto scheduled_value = [&]() { return _heating_schedule.value_at(now_s()); };

        switch (static_cast<Events>(event.id())) {
        case Events::VentilationFollowUp: {
            DBG("Ventilation: FollowUp.");
            ventilation_loop();
            evsys()->schedule(
                evsys()->event(Events::VentilationFollowUp, _cfg.ventilation.single_shot_length, Event::Data()));
            break;
        }
        case Events::VentilationStart: {
            DBG("Ventilation: Started.");
            _power.set_state(LogicalState::ON);
            _fan.fade_to(LogicalState::ON);

            const auto time_from_now = _cfg.ventilation.single_shot_length;
            DBGF("Ventilation: Scheduling VentilationStop in %u minutes.", time_m(time_from_now).raw<unsigned>());
            evsys()->schedule(evsys()->event(Events::VentilationStop, time_from_now, Event::Data()));
            DBG("Ventilation: Started. DONE");
            break;
        }
        case Events::VentilationStop: {
            DBG("Ventilation: Stopped.");
            _fan.fade_to(LogicalState::OFF);
            adjust_power_state();
            break;
        }
        case Events::HeatingAutoTuneHeater: {
            DBG("HeatingAutoTuneHeater: start.");
            break;
        }
        case Events::HeatingAutoTuneClimateControl: {
            DBG("HeatingAutoTuneClimateControl: start.");

            const auto autotune_setpoint = 27.0;
            const auto autotune_startpoint = autotune_setpoint - 0.5;

            _power.set_state(LogicalState::ON);

            // remove residual heat first
            _fan.set_value(LogicalState::ON);
            for (auto t = _temperature_humidity_probe.read_temperature(); t > autotune_startpoint;
                 t = _temperature_humidity_probe.read_temperature()) {
                DBGF("Ventilating for temperature %.2f C is lower than startpoint %.2f C before autotune "
                     "start",
                     t, autotune_startpoint);
                HAL::delay(time_s(1));
            }
            _fan.set_value(LogicalState::OFF);

            _climate_control.set_target_setpoint(autotune_setpoint);

            const auto process_setter = [&](double sp) {
                const auto new_setpoint = calculate_heater_setpoint(sp);
                DBGF("Autotune: Setting heater setpoint to: %.3f (response: %.3f, heater temp: %.2f)", new_setpoint, sp,
                     _heater.temperature());
                _heater.set_setpoint(new_setpoint);
            };
            const auto process_getter = [&]() { return _temperature_humidity_probe.read_temperature(); };
            const auto process_loop = [&]() {
                guard_temperature_limits();
                adjust_heater_fan_state();
                _heater.update();
            };
            _climate_control.autotune(PID::TuneConfig{.setpoint = autotune_setpoint,
                                                      .startpoint = autotune_startpoint,
                                                      .satured_at_start = true,
                                                      .cycles = 10},
                                      process_setter, process_getter, process_loop);
            adjust_power_state();
            break;
        }
        case Events::HeatingFollowUp: {
            // DBG("Heating: FollowUp.");
            heating_control_loop();
            _heater.update();
            evsys()->schedule(evsys()->event(Events::HeatingFollowUp, _cfg.heating.check_interval, Event::Data()));
            break;
        }
        case Events::HeatingCycleCheck: {
            const auto now = now_s();
            const auto next_setpoint = _heating_schedule.value_at(now);

            if (next_setpoint > 0 && _climate_control.setpoint() > 0) {
                DBGF("Heater: Check: heater is currently on, update value to setpoint: %.2f", next_setpoint);
                _climate_control.set_target_setpoint(next_setpoint);
            } else if (next_setpoint > 0 && _climate_control.setpoint() == 0) {
                DBGF("Heater: Check: heater is currently off, turn on and set setpoint to: %.2f", next_setpoint);
                evsys()->trigger(evsys()->event(Events::HeatingCycleStart, time_s(1), Event::Data()));
            } else if (next_setpoint == 0 && _climate_control.setpoint() > 0) {
                DBGF("Heater: Check: heater is currently on, turn off");
                evsys()->trigger(evsys()->event(Events::HeatingCycleStop, time_s(1), Event::Data()));
            }

            const auto time_until_next_check = _heating_schedule.start_of_next_block(now) - now;
            assert(_heating_schedule.start_of_next_block(now) > now);

            DBGF("Heating: Checked. Scheduling next check in %u m", time_m(time_until_next_check).printable());
            assert(time_until_next_check.raw<>() > 0);
            evsys()->schedule(evsys()->event(Events::HeatingCycleCheck, time_until_next_check, Event::Data()));
            break;
        }
        case Events::HeatingCycleStart: {
            DBG("Heating: Start.");
            _power.set_state(LogicalState::ON);

            const auto next_setpoint = _heating_schedule.value_at(now_s());

            DBGF("Heating: Setting climate control setpoint to %.2f", next_setpoint);
            _climate_control.set_target_setpoint(next_setpoint);
            break;
        }
        case Events::HeatingCycleStop: {
            //
            DBG("Heating: Stop.");
            _climate_control.set_target_setpoint(0);
            _heater.safe_shutdown();
            adjust_power_state();
            break;
        }
        default: assert(!"Event was not handled!"); break;
        };
    }

private:
    double calculate_heater_setpoint(const double pid_response) {
        return _heater.temperature() + pid_response
               - _cfg.heating.climate_control.output_upper_limit * (pid_response == 0);
    }

    ///
    void update_climate_controller() {
        const auto climate_temperature = _temperature_humidity_probe.read_temperature();
        _climate_control.new_reading(climate_temperature);
        // auto new_offset = _climate_control.response();
        // auto new_setpoint = _heater.temperature() + new_offset - (new_offset == 0);
        const auto response = _climate_control.response();
        const auto new_setpoint = calculate_heater_setpoint(response);

        if (_print_interval.expired()) {
            DBGF("Heating: outside T %.2f C,  climate T %.2f C, climate sp T %.2f C, surface T %.2f C, sp %.2f C, "
                 "response: %.2f C, new heater sp %.2f",
                 Clock::reference_temperature(), climate_temperature, _climate_control.setpoint(),
                 _heater.temperature(), _heater.setpoint(), response, new_setpoint);
        }
        _heater.set_setpoint(new_setpoint);
    }

    /// continuous checking of heating status
    void heating_control_loop() {
        guard_temperature_limits();
        adjust_heater_fan_state();

        if (_climate_control.setpoint() == 0.0) {
            // turn off the heater completely
            _heater.set_setpoint(0);
            return;
        }

        update_climate_controller();

        HAL::print(_heater.temperature());
        HAL::print("|");
        HAL::println(_temperature_humidity_probe.temperature());
    }

    /// continuous checking of ventilation status
    void ventilation_loop() {
        const auto climate_humidity = _temperature_humidity_probe.read_humidity();
        _ventilation_control.new_reading(climate_humidity);
        const auto next_state = _ventilation_control.response();
        const auto last_state = _fan.value() != 0.0;
        DBGF("Ventilation: current humidity %.2f %%, fan state: %i, next fan state: %i", climate_humidity, last_state,
             next_state);
        // the SRLatch maintains pacing, so we can safely call ventilation start here without timing checks
        if (next_state != last_state) {
            evsys()->trigger(evsys()->event(next_state == true ? Events::VentilationStart : Events::VentilationStop,
                                            _cfg.heating.check_interval, Event::Data()));
        }
    }

    /// If no power is needed, turn off the power
    void adjust_power_state() {
        if (_climate_control.setpoint() == 0 && _heater.setpoint() == 0 && _fan.value() == 0)
            _power.set_state(LogicalState::OFF);
    }

    /// Activate heater fan when heating or when cooling down
    void adjust_heater_fan_state() {
        if (_heater.state() != Heater::State::IDLE) {
            _heater_fan.set_value(LogicalState::ON);
        } else {
            _heater_fan.set_value(LogicalState::OFF);
        }
    }

    /// When any temperature readings are out of limits, shutdown the system
    void guard_temperature_limits() {
        //
        if (_heater.temperature() < MIN_SURFACE_TEMPERATURE || _heater.temperature() > MAX_SURFACE_TEMPERATURE) {
            dbg::throw_exception(spn::core::assertion_error("ClimateCountrol: Heater temperature out of limits"));
        }
        if (_temperature_humidity_probe.temperature() < MIN_INSIDE_TEMPERATURE
            || _temperature_humidity_probe.temperature() > MAX_INSIDE_TEMPERATURE) {
            dbg::throw_exception(spn::core::assertion_error("ClimateCountrol: Climate temperature out of limits"));
        }
        const auto outside_temperature = Clock::reference_temperature();
        if (outside_temperature < MIN_OUTSIDE_TEMPERATURE || outside_temperature > MAX_OUTSIDE_TEMPERATURE) {
            dbg::throw_exception(spn::core::assertion_error("ClimateCountrol: Outside temperature out of limits"));
        }
    }

private:
    const Config _cfg;

    AnalogueOutput _fan;

    AnalogueOutput _heater_fan;

    Heater _heater;

    Schedule _heating_schedule;

    // todo: abstract away sensor interface from implementation
    SHT31TempHumidityProbe _temperature_humidity_probe;

    PID _climate_control;
    SRLatch _ventilation_control;

    Relay _power;

    IntervalTimer _print_interval = IntervalTimer(time_s(10));
};

} // namespace kaskas::component