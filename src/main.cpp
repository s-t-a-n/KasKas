#include "kaskas/io/peripherals/DS18B20_Temp_Probe.hpp"
#include "kaskas/io/peripherals/analogue_input.hpp"
#include "kaskas/io/peripherals/analogue_output.hpp"
#include "kaskas/io/stack.hpp"
#include "kaskas/kaskas.hpp"

#include <spine/controller/pid.hpp>
#include <spine/core/debugging.hpp>
#include <spine/filter/implementations/ewma.hpp>

using spn::filter::EWMA;

using KasKas = kaskas::KasKas;
using HardwareStack = kaskas::io::HardwareStack;

static std::unique_ptr<KasKas> kk;
static std::shared_ptr<HardwareStack> hws;

using kaskas::Events;

using kaskas::io::Pump;
using spn::eventsystem::EventSystem;

using kaskas::io::DS18B20TempProbe;
using spn::controller::PID;
using BandPass = spn::filter::BandPass<double>;
using Heater = kaskas::io::Heater;
using kaskas::io::SHT31TempHumidityProbe;
using kaskas::io::clock::DS3231Clock;
using spn::controller::SRLatch;
using spn::core::time::Schedule;

// volatile uint8_t interruptor = 0;
// DigitalInput ub();

kaskas::io::HardwareStack* S = nullptr;

namespace Peripherals {
enum PeripheralsEnum {
    DS18B20,
    SHT31,
    DS3231,
    SOIL_MOISTURE_SENSOR,
    CLIMATE_FAN,
    HEATING_SURFACE_FAN,
    HEATING_ELEMENT,
    HEATING_POWER_RELAY,
    VIOLET_SPECTRUM_RELAY,
    BROAD_SPECTRUM_RELAY,
    SIZE,
};
}

void setup() {
    HAL::initialize(HAL::Config{.baudrate = 115200});
    HAL::println("Wake up");

    {
        using namespace kaskas::io;

        auto stack_cfg = HardwareStack::Config{.max_providers = Providers::SIZE, .max_peripherals = Peripherals::SIZE};

        auto sf = HardwareStackFactory(std::move(stack_cfg));

        {
            const auto cfg = SHT31TempHumidityProbe::Config{.sampling_interval = time_s(1)};
            auto peripheral = std::make_unique<SHT31TempHumidityProbe>(std::move(cfg));
            auto temperature_provider = std::make_shared<AnalogueSensor>(peripheral->temperature_provider());
            auto humidity_provider = std::make_shared<AnalogueSensor>(peripheral->humidity_provider());

            sf.hotload_provider(Providers::CLIMATE_TEMP, std::move(temperature_provider));
            sf.hotload_provider(Providers::CLIMATE_HUMIDITY, std::move(humidity_provider));
            sf.hotload_peripheral(Peripherals::SHT31, std::move(peripheral));
        }

        {
            const auto cfg = DS3231Clock::Config{.update_interval = time_s(1)};
            auto peripheral = std::make_unique<DS3231Clock>(std::move(cfg));
            auto clock_provider = std::make_shared<Clock>(peripheral->clock_provider());
            auto temperature_provider = std::make_shared<AnalogueSensor>(peripheral->temperature_provider());

            sf.hotload_provider(Providers::CLOCK, std::move(clock_provider));
            sf.hotload_provider(Providers::OUTSIDE_TEMP, std::move(temperature_provider));
            sf.hotload_peripheral(Peripherals::DS3231, std::move(peripheral));
        }

        {
            const auto cfg = AnalogueInputPeripheral::Config{.sampling_interval = time_s(10)};
            auto peripheral = std::make_unique<AnalogueInputPeripheral>(std::move(cfg));
            auto moisture_provider = std::make_shared<AnalogueSensor>(peripheral->analogue_value_provider());

            sf.hotload_provider(Providers::SOIL_MOISTURE, std::move(moisture_provider));
            sf.hotload_peripheral(Peripherals::SOIL_MOISTURE_SENSOR, std::move(peripheral));
        }

        {
            const auto cfg = AnalogueOutputPeripheral::Config{.pin = 5, .active_on_low = true};
            auto peripheral = std::make_unique<AnalogueOutputPeripheral>(std::move(cfg));
            auto state_provider = std::make_shared<AnalogueActuator>(peripheral->analogue_output_provider());

            sf.hotload_provider(Providers::CLIMATE_FAN, std::move(state_provider));
            sf.hotload_peripheral(Peripherals::CLIMATE_FAN, std::move(peripheral));
        }

        {
            const auto cfg = DS18B20TempProbe::Config{.pin = 3, .sampling_interval = time_ms(250)};
            auto peripheral = std::make_unique<DS18B20TempProbe>(std::move(cfg));
            auto provider = std::make_shared<AnalogueSensor>(peripheral->temperature_provider());

            sf.hotload_provider(Providers::HEATER_SURFACE_TEMP, std::move(provider));
            sf.hotload_peripheral(Peripherals::DS18B20, std::move(peripheral));
        }

        {
            const auto cfg = AnalogueOutputPeripheral::Config{.pin = 7, .active_on_low = true};
            auto peripheral = std::make_unique<AnalogueOutputPeripheral>(std::move(cfg));
            auto state_provider = std::make_shared<AnalogueActuator>(peripheral->analogue_output_provider());

            sf.hotload_provider(Providers::HEATING_SURFACE_FAN, std::move(state_provider));
            sf.hotload_peripheral(Peripherals::HEATING_SURFACE_FAN, std::move(peripheral));
        }

        {
            const auto cfg = AnalogueOutput::Config{.pin = 6, .active_on_low = true};
            auto peripheral = std::make_unique<AnalogueOutputPeripheral>(std::move(cfg));
            auto state_provider = std::make_shared<AnalogueActuator>(peripheral->analogue_output_provider());

            sf.hotload_provider(Providers::HEATING_ELEMENT, std::move(state_provider));
            sf.hotload_peripheral(Peripherals::HEATING_ELEMENT, std::move(peripheral));
        }

        {
            const auto cfg = Relay::Config{.pin_cfg = DigitalOutput::Config{.pin = 4, .active_on_low = true},
                                           .backoff_time = time_s(10)};
            auto peripheral = std::make_unique<Relay>(std::move(cfg));
            auto state_provider = std::make_shared<DigitalActuator>(peripheral->state_provider());

            sf.hotload_provider(Providers::HEATING_POWER, std::move(state_provider));
            sf.hotload_peripheral(Peripherals::HEATING_POWER_RELAY, std::move(peripheral));
        }

        {
            const auto cfg = Relay::Config{.pin_cfg = DigitalOutput::Config{.pin = A5, .active_on_low = true},
                                           .backoff_time = time_ms(1000)};
            auto peripheral = std::make_unique<Relay>(std::move(cfg));
            auto state_provider = std::make_shared<DigitalActuator>(peripheral->state_provider());

            sf.hotload_provider(Providers::VIOLET_SPECTRUM, std::move(state_provider));
            sf.hotload_peripheral(Peripherals::VIOLET_SPECTRUM_RELAY, std::move(peripheral));
        }

        {
            const auto cfg = Relay::Config{.pin_cfg = DigitalOutput::Config{.pin = 12, .active_on_low = true},
                                           .backoff_time = time_ms(1000)};
            auto peripheral = std::make_unique<Relay>(std::move(cfg));
            auto state_provider = std::make_shared<DigitalActuator>(peripheral->state_provider());

            sf.hotload_provider(Providers::BROAD_SPECTRUM, std::move(state_provider));
            sf.hotload_peripheral(Peripherals::BROAD_SPECTRUM_RELAY, std::move(peripheral));
        }

        hws = sf.stack();
        hws->initialize();

        // hws->digital_output(Providers::HEATING_POWER).set_state(LogicalState::ON);
        // hws->analogue_output(Providers::CLIMATE_FAN).fade_to(1.0);
    }

    {
        auto esc_cfg = EventSystem::Config{.events_count = static_cast<size_t>(Events::Size),
                                           .events_cap = 20,
                                           .handler_cap = 2,
                                           .delay_between_ticks = true,
                                           .min_delay_between_ticks = time_ms{1},
                                           .max_delay_between_ticks = time_ms{1000}};
        auto kk_cfg = KasKas::Config{.esc_cfg = esc_cfg, .component_cap = 16};
        kk = std::make_unique<KasKas>(hws, kk_cfg);
    }

    {
        using kaskas::component::ClimateControl;

        const auto sample_interval = time_ms(1000);
        const auto surface_offset = 10.0; // maximum offset above heater temperature
        const auto max_surface_setpoint = 40.0; // maximum allowed heater setpoint

        auto cc_cfg = ClimateControl::Config{
            .hws_power_idx = Providers::HEATING_POWER,
            .clock_idx = Providers::CLOCK,

            .ventilation = ClimateControl::Config::Ventilation{.hws_climate_fan_idx = Providers::CLIMATE_FAN,
                                                               .climate_humidity_idx = Providers::CLIMATE_HUMIDITY,
                                                               .single_shot_length = time_m(15),
                                                               .low_humidity = 70.0,
                                                               .high_humidity = 80.0,
                                                               .minimal_interval = time_m(15),
                                                               .maximal_interval = time_m(45)},
            .heating = ClimateControl::Config::Heating{
                .heating_element_fan_idx = Providers::HEATING_SURFACE_FAN,
                .heating_element_temp_sensor_idx = Providers::HEATER_SURFACE_TEMP,
                .climate_temp_sensor_idx = Providers::CLIMATE_TEMP,
                .outside_temp_idx = Providers::OUTSIDE_TEMP,
                .heater_cfg =
                    Heater::Config{.pid_cfg = PID::Config{.tunings = PID::Tunings{.Kp = 60.841, .Ki = 0.376, .Kd = 0.1},
                                                          .output_lower_limit = 0,
                                                          .output_upper_limit = 255,
                                                          .sample_interval = sample_interval},
                                   .max_heater_setpoint = max_surface_setpoint,
                                   .temperature_sensor_idx = Providers::HEATER_SURFACE_TEMP,
                                   .heating_element_idx = Providers::HEATING_ELEMENT},
                .schedule_cfg =
                    Schedule::Config{
                        .blocks = {Schedule::Block{.start = time_h(0), .duration = time_h(7), .value = 16.0}, // 16.0
                                   Schedule::Block{.start = time_h(7), .duration = time_h(2), .value = 18.0},
                                   Schedule::Block{.start = time_h(9), .duration = time_h(1), .value = 20.0},
                                   Schedule::Block{.start = time_h(10), .duration = time_h(1), .value = 22.0},
                                   Schedule::Block{.start = time_h(11), .duration = time_h(1), .value = 24.0},
                                   Schedule::Block{.start = time_h(12), .duration = time_h(8), .value = 27.0},
                                   Schedule::Block{.start = time_h(20), .duration = time_h(2), .value = 24.0}, // 24.0
                                   Schedule::Block{.start = time_h(22), .duration = time_h(2), .value = 16.0}}}, // 16.0
                .check_interval = time_s(1)}};

        auto ventilation = std::make_unique<ClimateControl>(*hws, cc_cfg);
        kk->hotload_component(std::move(ventilation));
    }

    {
        using kaskas::component::Growlights;
        auto growlights_cfg = Growlights::Config{.violet_spectrum_actuator_idx = Providers::VIOLET_SPECTRUM,
                                                 .broad_spectrum_actuator_idx = Providers::BROAD_SPECTRUM,
                                                 .clock_idx = Providers::CLOCK,
                                                 .starting_hour = time_h{6},
                                                 .duration_hours = time_h{16}};

        auto growlights = std::make_unique<Growlights>(*hws, growlights_cfg);
        kk->hotload_component(std::move(growlights));
    }

    {
        auto pump_cfg = Pump::Config{
            //
            .pump_cfg = AnalogueOutput::Config{.pin = 8, .active_on_low = true}, // NOP PORT
            .interrupt_cfg =
                Interrupt::Config{
                    .pin = 2, //
                    .mode = Interrupt::TriggerType::FALLING_EDGE, //
                    .pull_up = false, //
                },
            .ml_pulse_calibration = 25.8, //
            .reading_interval = time_ms(250), //
            .pump_timeout = time_s(10),
        };

        using kaskas::component::Fluidsystem;

        using GroundMoistureSensorFilter = Fluidsystem::GroundMoistureSensorFilter;
        using GroundMoistureSensorConfig = Fluidsystem::GroundMoistureSensor::Config;
        auto fluidsystem_cfg = Fluidsystem::Config{
            .pump_cfg = pump_cfg, //
            .ground_moisture_sensor_cfg =
                GroundMoistureSensorConfig{.sensor_cfg =
                                               AnalogueInput::Config{.pin = A1, .pull_up = false, .resolution = 10},
                                           .filter_cfg = GroundMoistureSensorFilter::Config{.K = 100, .invert = false}},
            .ground_moisture_threshold = 0.65, // 0.65

            .inject_dosis_ml = 250,
            .inject_check_interval = time_h(16), // time_h(16)
        };
        auto fluidsystem = std::make_unique<Fluidsystem>(*hws, fluidsystem_cfg);
        kk->hotload_component(std::move(fluidsystem));
    }

    {
        using kaskas::component::UI;
        auto ui_cfg =
            UI::Config{//
                       .signaltower_cfg =
                           Signaltower::Config{
                               //
                               .pin_red = DigitalOutput(DigitalOutput::Config{.pin = 10, .active_on_low = false}), //
                               .pin_yellow = DigitalOutput(DigitalOutput::Config{.pin = 9, .active_on_low = false}), //
                               .pin_green = DigitalOutput(DigitalOutput::Config{.pin = 8, .active_on_low = false}), //
                           },
                       .userbutton_cfg = DigitalInput::Config{.pin = PC13, .pull_up = false}};
        auto ui = std::make_unique<UI>(*hws, ui_cfg);
        kk->hotload_component(std::move(ui));
    }

    {
        using kaskas::component::Hardware;
        auto hardware_cfg = Hardware::Config{};

        auto hardware = std::make_unique<Hardware>(*hws, hardware_cfg);
        kk->hotload_component(std::move(hardware));
    }

    kk->initialize();
}
void loop() {
    if (kk == nullptr) {
        DBG("loop");
        // DBGF("interruptor: %i", interruptor);
        // DBGF("ub: %i", ub.state() == LogicalState::ON ? 1 : 0);
        HAL::delay(time_ms(1000));

        // const auto t = S->temperature(Providers::HEATER_SURFACE_TEMP);
        // DBGF("HEATER_SURFACE_TEMP : %f", t.value());
        //
        // const auto tcc = S->temperature(Providers::CLIMATE_TEMP);
        // DBGF("CLIMATE_TEMP : %f", tcc.value());
        //
        // const auto hcc = S->humidity(Providers::CLIMATE_HUMIDITY);
        // DBGF("CLIMATE_HUMIDITY : %f", hcc.value());
        //
        // const auto clock = S->clock(Providers::CLOCK);
        // const auto dt = clock.now();
        // const auto ot = S->temperature(Providers::OUTSIDE_TEMP);
        //
        // DBGF("CLOCK: %.2i:%.2i", dt.getHour(), dt.getMinute());
        // DBGF("CLOCK epoch: %i", static_cast<unsigned long>(clock.epoch()));
        // DBGF("OUTSIDE_TEMP : %f", ot.value());
        //
        // const auto sm = S->moisture(Providers::SOIL_MOISTURE);
        // DBGF("SOIL_MOISTURE : %f", sm.value());
        // S->update_all();

    } else {
        kk->loop();
    }
}

#if defined(UNITTEST) || defined(NATIVE)
int main(int argc, char** argv) {
    setup();
    while (true) {
        loop();
    }
    return 0;
}
#endif