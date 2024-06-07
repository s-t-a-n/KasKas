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

using kaskas::io::DS18B20TempProbe;

using kaskas::Events;
using kaskas::component::ClimateControl;
using kaskas::component::Fluidsystem;
using kaskas::component::Growlights;
using kaskas::component::UI;

using kaskas::io::Pump;
using spn::eventsystem::EventSystem;

using kaskas::io::DS18B20TempProbe;
using spn::controller::PID;
using BandPass = spn::filter::BandPass<double>;
using Heater = kaskas::io::Heater<DS18B20TempProbe>;
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
            const auto cfg = DS18B20TempProbe::Config{.pin = 3, .sampling_interval = time_ms(250)};
            auto peripheral = std::make_unique<DS18B20TempProbe>(std::move(cfg));
            auto provider = std::make_shared<AnalogueValue>(peripheral->temperature_provider());

            sf.hotload_provider(Providers::HEATER_SURFACE_TEMP, std::move(provider));
            sf.hotload_peripheral(Peripherals::DS18B20, std::move(peripheral));
        }

        {
            const auto cfg = SHT31TempHumidityProbe::Config{.sampling_interval = time_s(1)};
            auto peripheral = std::make_unique<SHT31TempHumidityProbe>(std::move(cfg));
            auto temperature_provider = std::make_shared<AnalogueValue>(peripheral->temperature_provider());
            auto humidity_provider = std::make_shared<AnalogueValue>(peripheral->humidity_provider());

            sf.hotload_provider(Providers::CLIMATE_TEMP, std::move(temperature_provider));
            sf.hotload_provider(Providers::CLIMATE_HUMIDITY, std::move(humidity_provider));
            sf.hotload_peripheral(Peripherals::SHT31, std::move(peripheral));
        }

        {
            const auto cfg = DS3231Clock::Config{.update_interval = time_s(1)};
            auto peripheral = std::make_unique<DS3231Clock>(std::move(cfg));
            auto clock_provider = std::make_shared<Clock>(peripheral->clock_provider());
            auto temperature_provider = std::make_shared<AnalogueValue>(peripheral->temperature_provider());

            sf.hotload_provider(Providers::CLOCK, std::move(clock_provider));
            sf.hotload_provider(Providers::OUTSIDE_TEMP, std::move(temperature_provider));
            sf.hotload_peripheral(Peripherals::DS3231, std::move(peripheral));
        }

        {
            const auto cfg = AnalogueInputPeripheral::Config{.sampling_interval = time_s(10)};
            auto peripheral = std::make_unique<AnalogueInputPeripheral>(std::move(cfg));
            auto moisture_provider = std::make_shared<AnalogueValue>(peripheral->analogue_value_provider());

            sf.hotload_provider(Providers::SOIL_MOISTURE, std::move(moisture_provider));
            sf.hotload_peripheral(Peripherals::SOIL_MOISTURE_SENSOR, std::move(peripheral));
        }

        {
            const auto cfg = Relay::Config{.pin_cfg = DigitalOutput::Config{.pin = 13, .active_on_low = true},
                                           .backoff_time = time_s(10)};
            auto peripheral = std::make_unique<Relay>(std::move(cfg));
            auto state_provider = std::make_shared<DigitalValue>(peripheral->state_provider());

            sf.hotload_provider(Providers::HEATING_POWER, std::move(state_provider));
            sf.hotload_peripheral(Peripherals::HEATING_POWER_RELAY, std::move(peripheral));
        }

        hws = sf.stack();
        hws->initialize();
    }

    {
        auto esc_cfg = EventSystem::Config{.events_count = static_cast<size_t>(Events::Size),
                                           .events_cap = 20,
                                           .handler_cap = 2,
                                           .delay_between_ticks = true,
                                           .min_delay_between_ticks = time_ms{1},
                                           .max_delay_between_ticks = time_ms{1000}};
        auto kk_cfg = KasKas::Config{.esc_cfg = esc_cfg, .component_cap = 16};
        kk = std::make_unique<KasKas>(kk_cfg);
    }

    {
        const auto sample_interval = time_ms(1000);
        const auto surface_offset = 10.0; // maximum offset above heater temperature
        const auto max_surface_setpoint = 40.0; // maximum allowed heater setpoint

        auto cc_cfg = ClimateControl::Config{
            .power_cfg = Relay::Config{.pin_cfg = DigitalOutput::Config{.pin = 4, .active_on_low = true},
                                       .backoff_time = time_s(10)},
            .ventilation =
                ClimateControl::Config::Ventilation{.fan_cfg = AnalogueOutput::Config{.pin = 5, .active_on_low = true},
                                                    .single_shot_length = time_m(15),
                                                    .low_humidity = 70.0,
                                                    .high_humidity = 80.0,
                                                    .minimal_interval = time_m(15),
                                                    .maximal_interval = time_m(45)},
            .heating = ClimateControl::Config::Heating{
                .fan_cfg = AnalogueOutput::Config{.pin = 7, .active_on_low = true},
                .heater_cfg =
                    Heater::Config{.pid_cfg = PID::Config{.tunings = PID::Tunings{.Kp = 60.841, .Ki = 0.376, .Kd = 0.1},
                                                          .output_lower_limit = 0,
                                                          .output_upper_limit = 255,
                                                          .sample_interval = sample_interval},
                                   .heater_cfg = AnalogueOutput::Config{.pin = 6, .active_on_low = true},
                                   .max_heater_setpoint = max_surface_setpoint,
                                   .tempprobe_cfg = DS18B20TempProbe::Config{.pin = 3},
                                   .tempprobe_filter_cfg = BandPass::Config{.mode = BandPass::Mode::RELATIVE,
                                                                            .mantissa = 1,
                                                                            .decades = 0.01,
                                                                            .offset = 0}},
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

        auto ventilation = std::make_unique<ClimateControl>(cc_cfg);
        kk->hotload_component(std::move(ventilation));
    }

    {
        auto growlights_cfg = Growlights::Config{
            .violet_spectrum_cfg =
                Relay::Config{
                    .pin_cfg = DigitalOutput::Config{.pin = A5, .active_on_low = true}, //
                    .backoff_time = time_ms(1000) //
                },
            .broad_spectrum_cfg =
                Relay::Config{
                    .pin_cfg = DigitalOutput::Config{.pin = 12, .active_on_low = true}, //
                    .backoff_time = time_ms(1000) //
                }, //
            .starting_hour = time_h{6}, //
            .duration_hours = time_h{16} //
        };

        auto growlights = std::make_unique<Growlights>(growlights_cfg);
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
        auto fluidsystem = std::make_unique<Fluidsystem>(fluidsystem_cfg);
        kk->hotload_component(std::move(fluidsystem));
    }

    {
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
        auto ui = std::make_unique<UI>(ui_cfg);
        kk->hotload_component(std::move(ui));
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