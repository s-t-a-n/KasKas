#pragma once

#include "kaskas/component.hpp"
#include "kaskas/events.hpp"
#include "kaskas/io/controllers/heater.hpp"
#include "kaskas/io/peripherals/DS18B20_Temp_Probe.hpp"
#include "kaskas/io/peripherals/DS3231_RTC_EEPROM.hpp"
#include "kaskas/io/peripherals/SHT31_TempHumidityProbe.hpp"
#include "kaskas/io/peripherals/relay.hpp"
#include "kaskas/io/providers/clock.hpp"

#include <spine/controller/pid.hpp>
#include <spine/controller/sr_latch.hpp>
#include <spine/core/debugging.hpp>
#include <spine/core/types.hpp>
#include <spine/core/utils/string.hpp>
#include <spine/eventsystem/eventsystem.hpp>
#include <spine/filter/implementations/bandpass.hpp>
#include <spine/filter/implementations/ewma.hpp>
#include <spine/platform/hal.hpp>
#include <spine/structure/time/schedule.hpp>

namespace kaskas::component {

namespace detail {
inline double inverted(double value, double base = 100.0) { return base - value; }
} // namespace detail

class ClimateControl final : public kaskas::Component {
public:
    using PID = spn::controller::PID;
    using Schedule = spn::structure::time::Schedule;
    using Heater = kaskas::io::Heater;
    using Event = spn::eventsystem::Event;

    struct Config {
        static constexpr std::string_view name = "ClimateControl";
        io::HardwareStack::Idx hws_power_idx;
        io::HardwareStack::Idx clock_idx;

        struct Ventilation {
            io::HardwareStack::Idx hws_climate_fan_idx;
            io::HardwareStack::Idx climate_humidity_idx;

            PID::Config climate_fan_pid;
            double minimal_duty_cycle = 0; // normalized value where 0.5 means 50% dutycycle
            Schedule::Config schedule_cfg;
            k_time_s check_interval;
        } ventilation;
        struct Heating {
            io::HardwareStack::Idx heating_element_fan_idx;
            io::HardwareStack::Idx heating_element_temp_sensor_idx;
            io::HardwareStack::Idx climate_temp_sensor_idx;
            io::HardwareStack::Idx ambient_temp_idx;

            Heater::Config heater_cfg;
            Schedule::Config schedule_cfg;
            k_time_s check_interval;
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

        // starts autotuning immediately!
        //        evsys()->schedule(evsys()->event(Events::HeatingAutoTune, k_time_s(1), Event::Data(20.0)));
        //        evsys()->schedule(evsys()->event(Events::VentilationAutoTune, k_time_s(1), Event::Data(70.0)));

        auto time_from_now = k_time_s(15);
        DBG("ClimateControl: Scheduling ventilation check in %li seconds.", time_from_now.raw());
        evsys()->schedule(evsys()->event(Events::VentilationCycleCheck, time_from_now));
        evsys()->schedule(evsys()->event(Events::VentilationFollowUp, time_from_now));

        time_from_now += k_time_s(5);
        DBG("ClimateControl: Scheduling heating check in %li seconds.", time_from_now.raw());
        evsys()->schedule(evsys()->event(Events::HeatingCycleCheck, time_from_now));
        evsys()->schedule(evsys()->event(Events::HeatingFollowUp, time_from_now));
    }

    void safe_shutdown(State state) override {
        DBG("ClimateControl: Shutting down");
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
            spn_assert(_clock.is_ready());
            const auto now_dt = _clock.now();
            return k_time_s(k_time_h(now_dt.getHour())) + k_time_m(now_dt.getMinute());
        };

        switch (static_cast<Events>(event.id())) {
        case Events::VentilationFollowUp: {
            ventilation_control_loop();
            evsys()->schedule(evsys()->event(Events::VentilationFollowUp, _cfg.ventilation.check_interval));
            break;
        }
        case Events::VentilationCycleCheck: {
            const auto now = now_s();
            const auto next_setpoint = _ventilation_schedule.value_at(now);

            if (next_setpoint > 0 && _ventilation_control.setpoint() > 0) {
                LOG("ClimateControl: Ventilation is currently on, UPDATE value to humidity setpoint: %.2f",
                    next_setpoint);
                _ventilation_control.set_target_setpoint(detail::inverted(next_setpoint));
            } else if (next_setpoint > 0 && _ventilation_control.setpoint() == 0) {
                LOG("ClimateControl: Ventilation is currently off, turn ON and set humidity setpoint to: %.2f",
                    next_setpoint);
                evsys()->trigger(evsys()->event(Events::VentilationCycleStart, k_time_s(1)));
            } else if (next_setpoint == 0 && _heater.setpoint() > 0) {
                LOG("ClimateControl: Ventilation is currently on, turn OFF");
                evsys()->trigger(evsys()->event(Events::VentilationCycleStop, k_time_s(1)));
            }

            const auto time_until_next_check = _ventilation_schedule.start_of_next_block(now) - now;
            spn_assert(_ventilation_schedule.start_of_next_block(now) > now);

            DBG("ClimateControl: Scheduling next ventilation check in %li m", k_time_m(time_until_next_check).raw());
            spn_assert(time_until_next_check.raw<>() > 0);
            evsys()->schedule(evsys()->event(Events::VentilationCycleCheck, time_until_next_check));
            break;
        }
        case Events::VentilationCycleStart: {
            _power.set_state(LogicalState::ON);
            const auto next_setpoint = _ventilation_schedule.value_at(now_s());

            DBG("ClimateControl: Starting ventilation cycle. Setting humidity setpoint to %.2f", next_setpoint);
            _ventilation_control.set_target_setpoint(detail::inverted(next_setpoint));
            break;
        }
        case Events::VentilationCycleStop: {
            DBG("ClimateControl: Stopped ventilation cycle.");
            _climate_fan.creep_to(LogicalState::OFF, _cfg.ventilation.check_interval);
            adjust_power_state();
            break;
        }
        case Events::VentilationAutoTune: {
            if (!event.data().has_value() || event.data().value() == 0) {
                ERR("ClimateControl: tried to start ventilation autotuning without setpoint provided");
                return;
            }
            LOG("ClimateControl: starting ventilation autotune..");

            const auto sp = event.data().value();
            const auto autotune_setpoint = detail::inverted(sp);

            _power.set_state(LogicalState::ON);
            _climate_fan.fade_to(LogicalState::OFF);

            const auto process_getter = [&]() { return detail::inverted(_climate_humidity.value()); };
            const auto process_setter = [&](double value) { _climate_fan.fade_to(value / 100.0); };
            const auto process_loop = [&]() { _hws.update_all(); };

            const auto tunings = _ventilation_control.autotune(
                PID::TuneConfig{.setpoint = autotune_setpoint,
                                .startpoint = 0,
                                .hysteresis = 0.3,
                                .satured_at_start = false,
                                .cycles = 30,
                                .aggressiveness = spn::controller::PIDAutotuner::Aggressiveness::BasicPID},
                process_setter, process_getter, process_loop);
            _climate_fan.fade_to(LogicalState::OFF);
            adjust_power_state();
            LOG("ClimateControl: Ventilation autotuning complete, results: kp: %f, ki: %f, kd: %f", tunings.Kp,
                tunings.Ki, tunings.Kd);
            _ventilation_control.set_tunings(tunings);
            break;
        }
        case Events::HeatingAutoTune: {
            if (!event.data().has_value() || event.data().value() == 0) {
                ERR("ClimateControl: tried to start heating autotuning without setpoint provided");
                return;
            }
            LOG("ClimateControl: starting heating autotune..");

            const auto autotune_setpoint = event.data().value();
            const auto autotune_startpoint = autotune_setpoint - 1.0;

            _power.set_state(LogicalState::ON);

            // remove residual heat first
            _climate_fan.creep_stop();
            _climate_fan.fade_to(LogicalState::ON);
            for (auto t = _climate_temperature.value(); t > autotune_startpoint; t = _climate_temperature.value()) {
                DBG("ClimateControl: Ventilating for temperature %.2f C is lower than startpoint %.2f C before "
                    "autotune start",
                    t, autotune_startpoint);
                _hws.update_all();
                HAL::delay(k_time_s(1));
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
            heating_control_loop();
            evsys()->schedule(evsys()->event(Events::HeatingFollowUp, _cfg.heating.check_interval));
            break;
        }
        case Events::HeatingCycleCheck: {
            const auto now = now_s();
            const auto next_setpoint = _heating_schedule.value_at(now);

            if (next_setpoint > 0 && _heater.setpoint() > 0) {
                LOG("ClimateControl: Heater is currently on, UPDATE value to setpoint: %.2f", next_setpoint);
                _heater.set_target_setpoint(next_setpoint);
            } else if (next_setpoint > 0 && _heater.setpoint() == 0) {
                LOG("ClimateControl: Heater is currently off, turn ON and set setpoint to: %.2f", next_setpoint);
                evsys()->trigger(evsys()->event(Events::HeatingCycleStart, k_time_s(1)));
            } else if (next_setpoint == 0 && _heater.setpoint() > 0) {
                LOG("ClimateControl: Heater is currently on, turn OFF");
                evsys()->trigger(evsys()->event(Events::HeatingCycleStop, k_time_s(1)));
            }

            const auto time_until_next_check = _heating_schedule.start_of_next_block(now) - now;
            spn_assert(_heating_schedule.start_of_next_block(now) > now);

            DBG("ClimateControl: Scheduling next heating check in %li m", k_time_m(time_until_next_check).raw());
            spn_assert(time_until_next_check.raw<>() > 0);
            evsys()->schedule(evsys()->event(Events::HeatingCycleCheck, time_until_next_check));
            break;
        }
        case Events::HeatingCycleStart: {
            _power.set_state(LogicalState::ON);

            const auto next_setpoint = _heating_schedule.value_at(now_s());
            DBG("ClimateControl: Starting heating cycle. Setting climate control setpoint to %.2f", next_setpoint);

            _heater.set_target_setpoint(next_setpoint);
            break;
        }
        case Events::HeatingCycleStop: {
            DBG("ClimateControl: Stopping heating cycle.");
            _heater.set_target_setpoint(LogicalState::OFF);
            _heater.safe_shutdown();
            adjust_power_state();
            break;
        }
        default: spn_assert(!"Event was not handled!"); break;
        };
    }

    std::unique_ptr<prompt::RPCRecipe> rpc_recipe() override {
        using namespace prompt;
        auto model = std::make_unique<RPCRecipe>(RPCRecipe(
            _cfg.name,
            {
                RPCModel("heaterAutotune",
                         [this](const OptStringView& setpoint) {
                             if (!setpoint) return RPCResult(RPCResult::Status::BAD_INPUT);
                             evsys()->trigger(Events::HeatingAutoTune,
                                              Event::Data(spn::core::utils::to_double(*setpoint)));
                             return RPCResult(RPCResult::Status::OK);
                         }),
                RPCModel("heaterStatus",
                         [this](const OptStringView&) {
                             return RPCResult(std::string(magic_enum::enum_name(_heater.state())));
                         }),
                RPCModel("heaterSetpoint",
                         [this](const OptStringView&) { return RPCResult(std::to_string(_heater.setpoint())); }),
                RPCModel("ventilationAutotune",
                         [this](const OptStringView& setpoint) {
                             if (!setpoint) return RPCResult(RPCResult::Status::BAD_INPUT);
                             evsys()->trigger(Events::VentilationAutoTune,
                                              Event::Data(spn::core::utils::to_double(*setpoint)));
                             return RPCResult(RPCResult::Status::OK);
                         }),
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
    using LogicalState = spn::core::LogicalState;

    /// continuous checking of heating status
    void heating_control_loop() {
        adjust_heater_fan_state();

        if (_heater.setpoint() == 0.0) return;
        _heater.update();
        if (_heater.state() != Heater::State::IDLE) _power.set_state(LogicalState::ON);
    }

    /// continuous checking of ventilation status
    void ventilation_control_loop() {
        const auto climate_humidity = _climate_humidity.value();
        _ventilation_control.new_reading(detail::inverted(climate_humidity));
        const auto normalized_response = _ventilation_control.response() / 100;
        auto adjusted_normalized_response = normalized_response;

        if (_heater.state() == io::Heater::State::HEATING) { // no ventilation during heating
            adjusted_normalized_response = 0;
        } else if (_heater.state() == io::Heater::State::STEADY_STATE) { // reduce ventilation in proportion to error
            spn_assert(_cfg.heating.heater_cfg.steady_state_hysteresis != 0);
            if (_heater.temperature() < _heater.setpoint())
                adjusted_normalized_response -= _heater.error() / _cfg.heating.heater_cfg.steady_state_hysteresis;
        }

        if (adjusted_normalized_response > _cfg.ventilation.minimal_duty_cycle) {
            _climate_fan.creep_to(adjusted_normalized_response, _cfg.ventilation.check_interval);
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
    using Component = kaskas::Component;
    using Events = kaskas::Events;
    using EventSystem = spn::core::EventSystem;
    using IntervalTimer = spn::structure::time::IntervalTimer;

    using DS18B20TempProbe = kaskas::io::DS18B20TempProbe;
    using SHT31TempHumidityProbe = kaskas::io::SHT31TempHumidityProbe;
    using SRLatch = spn::controller::SRLatch;

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

    IntervalTimer _print_interval = IntervalTimer(k_time_s(10));
};

} // namespace kaskas::component
