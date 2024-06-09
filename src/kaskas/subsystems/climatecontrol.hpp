#pragma once

#include "../io/peripherals/relay.hpp"
#include "kaskas/component.hpp"
#include "kaskas/events.hpp"
#include "kaskas/io/controllers//heater.hpp"
#include "kaskas/io/peripherals/DS18B20_Temp_Probe.hpp"
#include "kaskas/io/peripherals/DS3231_RTC_EEPROM.hpp"
#include "kaskas/io/peripherals/SHT31_TempHumidityProbe.hpp"
#include "kaskas/io/peripherals/fan.hpp"
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

class ClimateControl final : public kaskas::Component {
public:
    struct Config {
        io::HardwareStack::Idx hws_power_idx;
        io::HardwareStack::Idx clock_idx;

        struct Ventilation {
            io::HardwareStack::Idx hws_climate_fan_idx;
            io::HardwareStack::Idx climate_humidity_idx;

            time_s minimal_on_duration; // duration that ventilation will be on at minimum in a single go
            time_s maximal_on_duration; // duration that ventilation will be on at maximum in a single go
            double low_humidity;
            double high_humidity;
            time_s minimal_interval; // minimal interval between consecutive shots
            time_s maximal_interval; // maximal length of time that no ventilation takes place
        } ventilation;
        struct Heating {
            io::HardwareStack::Idx heating_element_fan_idx;
            io::HardwareStack::Idx heating_element_temp_sensor_idx;
            io::HardwareStack::Idx climate_temp_sensor_idx;
            io::HardwareStack::Idx outside_temp_idx;

            Heater::Config heater_cfg;
            Schedule::Config schedule_cfg;
            time_s check_interval;
        } heating;
    };

public:
    ClimateControl(io::HardwareStack& hws, const Config& cfg) : ClimateControl(hws, nullptr, cfg) {}
    ClimateControl(io::HardwareStack& hws, EventSystem* evsys, const Config& cfg)
        : Component(evsys, hws), _cfg(std::move(cfg)), _clock(_hws.clock(_cfg.clock_idx)),
          _outside_temp_sensor(_hws.analog_sensor(_cfg.heating.outside_temp_idx)),
          _climate_fan(_hws.analogue_actuator(_cfg.ventilation.hws_climate_fan_idx)),
          _climate_humidity(_hws.analog_sensor(_cfg.ventilation.climate_humidity_idx)),
          _climate_temperature(_hws.analog_sensor(_cfg.heating.climate_temp_sensor_idx)),
          _heating_element_fan(_hws.analogue_actuator(_cfg.heating.heating_element_fan_idx)),
          _heating_element_sensor(_hws.analog_sensor(_cfg.heating.heating_element_temp_sensor_idx)),
          _heater(std::move(cfg.heating.heater_cfg), _hws), _heating_schedule(std::move(_cfg.heating.schedule_cfg)),
          _ventilation_control(SRLatch::Config{.high = _cfg.ventilation.high_humidity,
                                               .low = _cfg.ventilation.low_humidity,
                                               .minimal_on_time = _cfg.ventilation.minimal_on_duration,
                                               .maximal_on_time = _cfg.ventilation.maximal_on_duration,
                                               .minimal_off_time = _cfg.ventilation.minimal_interval,
                                               .maximal_off_time = _cfg.ventilation.maximal_interval}),
          _power(_hws.digital_actuator(_cfg.hws_power_idx)){};

    void initialize() override {
        //
        _heater.set_temperature_source(Heater::TemperatureSource::CLIMATE);
        _heater.initialize();
        _ventilation_control.initialize();

        evsys()->attach(Events::VentilationFollowUp, this);
        evsys()->attach(Events::VentilationStart, this);
        evsys()->attach(Events::VentilationStop, this);
        evsys()->attach(Events::HeatingAutoTune, this);
        evsys()->attach(Events::HeatingFollowUp, this);
        evsys()->attach(Events::HeatingCycleCheck, this);
        evsys()->attach(Events::HeatingCycleStart, this);
        evsys()->attach(Events::HeatingCycleStop, this);

        // starts autotuning now!
        // evsys()->schedule(evsys()->event(Events::HeatingAutoTune, time_s(1), Event::Data()));

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

            const auto intervals = std::initializer_list<time_s>{
                _cfg.ventilation.minimal_on_duration, _cfg.ventilation.maximal_on_duration,
                _cfg.ventilation.minimal_interval, _cfg.ventilation.maximal_interval};
            const auto shortest_interval = std::min_element(intervals.begin(), intervals.end());
            evsys()->schedule(evsys()->event(Events::VentilationFollowUp, *shortest_interval, Event::Data()));
            break;
        }
        case Events::VentilationStart: {
            DBG("Ventilation: Started.");
            _power.set_state(LogicalState::ON);
            _climate_fan.fade_to(LogicalState::ON);

            // const auto time_from_now = _cfg.ventilation.minimal_on_duration;
            // DBGF("Ventilation: Scheduling VentilationStop in %u minutes.", time_m(time_from_now).raw<unsigned>());
            // evsys()->schedule(evsys()->event(Events::VentilationStop, time_from_now, Event::Data()));
            break;
        }
        case Events::VentilationStop: {
            DBG("Ventilation: Stopped.");
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
            evsys()->schedule(evsys()->event(Events::HeatingFollowUp, _cfg.heating.check_interval, Event::Data()));
            break;
        }
        case Events::HeatingCycleCheck: {
            const auto now = now_s();
            const auto next_setpoint = _heating_schedule.value_at(now);

            if (next_setpoint > 0 && _heater.setpoint() > 0) {
                DBGF("Heater: Check: heater is currently on, update value to setpoint: %.2f", next_setpoint);
                _heater.set_setpoint(next_setpoint);
            } else if (next_setpoint > 0 && _heater.setpoint() == 0) {
                DBGF("Heater: Check: heater is currently off, turn on and set setpoint to: %.2f", next_setpoint);
                evsys()->trigger(evsys()->event(Events::HeatingCycleStart, time_s(1), Event::Data()));
            } else if (next_setpoint == 0 && _heater.setpoint() > 0) {
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
            _heater.set_setpoint(next_setpoint);
            break;
        }
        case Events::HeatingCycleStop: {
            //
            DBG("Heating: Stop.");
            _heater.set_setpoint(LogicalState::OFF);
            _heater.safe_shutdown();
            adjust_power_state();
            break;
        }
        default: assert(!"Event was not handled!"); break;
        };
    }

private:
    /// continuous checking of heating status
    void heating_control_loop() {
        adjust_heater_fan_state();

        if (_heater.setpoint() == 0.0) {
            return;
        }

        if (_print_interval.expired()) {
            DBGF("Heating: outsideT %.2fC, surfaceT %.2f, climateT %.2fC, sp T %.2f C", _outside_temp_sensor.value(),
                 _heating_element_sensor.value(), _climate_temperature.value(), _heater.setpoint());
        }

        _heater.update();

        if (_heater.state() != Heater::State::IDLE) {
            _power.set_state(LogicalState::ON);
        }

        HAL::print(_heating_element_sensor.value());
        HAL::print("|");
        HAL::println(_climate_temperature.value());
    }

    /// continuous checking of ventilation status
    void ventilation_loop() {
        const auto climate_humidity = _climate_humidity.value();
        _ventilation_control.new_reading(climate_humidity);
        auto next_state = _ventilation_control.response();
        const auto last_state = _climate_fan.value() != 0.0;

        DBGF("Ventilation: Humidity %.2f%%", _climate_humidity.value());

        // When the current climate temperature is above the setpoint temperature and the heater is still cooling down
        // -> Remove residual heat to hasten the fall of temperature
        if (_climate_temperature.value() > _heater.setpoint() && _heater.state() == Heater::State::COOLING_DOWN) {
            next_state = true;
        }

        // the SRLatch maintains pacing, so we can safely call ventilation start/stop here without timing checks
        if (next_state != last_state) {
            DBGF("Ventilation: current humidity %.2f %%, fan state: %i, next fan state: %i", climate_humidity,
                 last_state, next_state);
            evsys()->trigger(evsys()->event(next_state == true ? Events::VentilationStart : Events::VentilationStop,
                                            _cfg.heating.check_interval, Event::Data()));
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

    const io::AnalogueSensor& _outside_temp_sensor;

    io::AnalogueActuator& _climate_fan;
    const io::AnalogueSensor& _climate_humidity;
    const io::AnalogueSensor& _climate_temperature;

    Heater _heater;
    io::AnalogueActuator& _heating_element_fan;
    const io::AnalogueSensor& _heating_element_sensor;

    Schedule _heating_schedule;

    SRLatch _ventilation_control;

    io::DigitalActuator& _power;

    IntervalTimer _print_interval = IntervalTimer(time_s(10));
};

} // namespace kaskas::component