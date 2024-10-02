#if !defined(UNITTEST)

#    include "kaskas/io/hardware_stack.hpp"
#    include "kaskas/io/peripherals/DS18B20_Temp_Probe.hpp"
#    include "kaskas/io/peripherals/analogue_input.hpp"
#    include "kaskas/io/peripherals/analogue_output.hpp"
#    include "kaskas/kaskas.hpp"
#    include "kaskas/prompt/prompt.hpp"

#    include <spine/controller/pid.hpp>
#    include <spine/core/debugging.hpp>
#    include <spine/filter/implementations/bandpass.hpp>
#    include <spine/filter/implementations/ewma.hpp>
#    include <spine/filter/implementations/invert.hpp>
#    include <spine/filter/implementations/mapped_range.hpp>

using KasKas = kaskas::KasKas;
using HardwareStack = kaskas::io::HardwareStack;

static std::unique_ptr<KasKas> kk;
static std::shared_ptr<HardwareStack> hws;

using kaskas::Events;
using kaskas::io::DS18B20TempProbe;
using kaskas::io::Pump;
using kaskas::io::SHT31TempHumidityProbe;
using kaskas::io::clock::DS3231Clock;
using spn::controller::PID;
using spn::controller::SRLatch;
using spn::core::time::Schedule;
using spn::eventsystem::EventSystem;

using BandPass = spn::filter::BandPass<double>;
using EWMA = spn::filter::EWMA<double>;
using MappedRange = spn::filter::MappedRange<double>;
using Invert = spn::filter::Invert<double>;
using Heater = kaskas::io::Heater;

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
    PUMP_RELAY,
    SIZE,
};
}

using namespace kaskas;

void setup() {
    HAL::delay(time_s(2)); // give time for console to attach before first output
    HAL::initialize(HAL::Config{.baudrate = 115200});
    LOG("Wake up");

    {
        using namespace kaskas::io;

        auto stack_cfg = HardwareStack::Config{
            .alias = "IO", .max_providers = ENUM_IDX(DataProviders::SIZE), .max_peripherals = Peripherals::SIZE};

        auto sf = HardwareStackFactory(std::move(stack_cfg));

        // Initialize and hotload peripherals and providers
        {
            const auto cfg = SHT31TempHumidityProbe::Config{.sampling_interval = time_s(1)};
            auto peripheral = std::make_unique<SHT31TempHumidityProbe>(std::move(cfg));
            auto temperature_provider = std::make_shared<AnalogueSensor>(peripheral->temperature_provider());
            auto humidity_provider = std::make_shared<AnalogueSensor>(peripheral->humidity_provider());

            sf.hotload_provider(DataProviders::CLIMATE_TEMP, std::move(temperature_provider));
            sf.hotload_provider(DataProviders::CLIMATE_HUMIDITY, std::move(humidity_provider));
            sf.hotload_peripheral(Peripherals::SHT31, std::move(peripheral));
        }

        {
            const auto cfg = DS3231Clock::Config{.update_interval = time_s(1)};
            auto peripheral = std::make_unique<DS3231Clock>(std::move(cfg));
            auto clock_provider = std::make_shared<Clock>(peripheral->clock_provider());
            auto temperature_provider = std::make_shared<AnalogueSensor>(peripheral->temperature_provider());

            sf.hotload_provider(DataProviders::CLOCK, std::move(clock_provider));
            sf.hotload_provider(DataProviders::AMBIENT_TEMP, std::move(temperature_provider));
            sf.hotload_peripheral(Peripherals::DS3231, std::move(peripheral));
        }

        {
            // constexpr double moisture_sensor_limits[] = {0.4, 0.6}; // experimentally obtained, low = wet, high = dry
            constexpr double moisture_sensor_limits[] = {0.2, 0.7}; // experimentally obtained, low = wet, high = dry
            // constexpr double moisture_sensor_limits[] = {0.0, 1.0}; // experimentally obtained, low = wet, high = dry
            const auto cfg =
                AnalogueInputPeripheral::Config{.input_cfg = HAL::AnalogueInput::Config{.pin = A1, .pull_up = false},
                                                .sampling_interval = time_s(60),
                                                .number_of_filters = 4};
            auto peripheral = std::make_unique<AnalogueInputPeripheral>(std::move(cfg));
            peripheral->attach_filter(BandPass::Broad());
            peripheral->attach_filter(EWMA::Long());
            peripheral->attach_filter(Invert::NormalizedInverter());
            peripheral->attach_filter(MappedRange::Percentage(moisture_sensor_limits[0], moisture_sensor_limits[1]));
            auto moisture_provider = std::make_shared<AnalogueSensor>(peripheral->analogue_value_provider());

            sf.hotload_provider(DataProviders::SOIL_MOISTURE, std::move(moisture_provider));
            sf.hotload_peripheral(Peripherals::SOIL_MOISTURE_SENSOR, std::move(peripheral));
        }

        {
            const auto sample_speed = time_ms(50);
            const auto cfg = AnalogueOutputPeripheral::Config{.pin = 5, .active_on_low = true};
            auto peripheral = std::make_unique<AnalogueOutputPeripheral>(std::move(cfg), sample_speed);
            auto state_provider = std::make_shared<AnalogueActuator>(peripheral->analogue_output_provider());

            sf.hotload_provider(DataProviders::CLIMATE_FAN, std::move(state_provider));
            sf.hotload_peripheral(Peripherals::CLIMATE_FAN, std::move(peripheral));
        }

        {
            const auto cfg = DS18B20TempProbe::Config{.pin = 3, .sampling_interval = time_ms(1000)};
            auto peripheral = std::make_unique<DS18B20TempProbe>(std::move(cfg));
            auto provider = std::make_shared<AnalogueSensor>(peripheral->temperature_provider());

            sf.hotload_provider(DataProviders::HEATING_SURFACE_TEMP, std::move(provider));
            sf.hotload_peripheral(Peripherals::DS18B20, std::move(peripheral));
        }

        {
            const auto cfg = AnalogueOutputPeripheral::Config{.pin = 7, .active_on_low = true};
            auto peripheral = std::make_unique<AnalogueOutputPeripheral>(std::move(cfg));
            auto state_provider = std::make_shared<AnalogueActuator>(peripheral->analogue_output_provider());

            sf.hotload_provider(DataProviders::HEATING_SURFACE_FAN, std::move(state_provider));
            sf.hotload_peripheral(Peripherals::HEATING_SURFACE_FAN, std::move(peripheral));
        }

        {
            const auto cfg = AnalogueOutput::Config{.pin = 6, .active_on_low = true};
            auto peripheral = std::make_unique<AnalogueOutputPeripheral>(std::move(cfg));
            auto state_provider = std::make_shared<AnalogueActuator>(peripheral->analogue_output_provider());

            sf.hotload_provider(DataProviders::HEATING_ELEMENT, std::move(state_provider));
            sf.hotload_peripheral(Peripherals::HEATING_ELEMENT, std::move(peripheral));
        }

        {
            const auto cfg = Relay::Config{.pin_cfg = DigitalOutput::Config{.pin = 4, .active_on_low = true},
                                           .backoff_time = time_s(10)};
            auto peripheral = std::make_unique<Relay>(std::move(cfg));
            auto state_provider = std::make_shared<DigitalActuator>(peripheral->state_provider());

            sf.hotload_provider(DataProviders::HEATING_POWER, std::move(state_provider));
            sf.hotload_peripheral(Peripherals::HEATING_POWER_RELAY, std::move(peripheral));
        }

        {
            const auto cfg = Relay::Config{.pin_cfg = DigitalOutput::Config{.pin = A5, .active_on_low = true},
                                           .backoff_time = time_ms(1000)};
            auto peripheral = std::make_unique<Relay>(std::move(cfg));
            auto state_provider = std::make_shared<DigitalActuator>(peripheral->state_provider());

            sf.hotload_provider(DataProviders::REDBLUE_SPECTRUM, std::move(state_provider));
            sf.hotload_peripheral(Peripherals::VIOLET_SPECTRUM_RELAY, std::move(peripheral));
        }

        {
            const auto cfg = Relay::Config{.pin_cfg = DigitalOutput::Config{.pin = 12, .active_on_low = true},
                                           .backoff_time = time_ms(1000)};
            auto peripheral = std::make_unique<Relay>(std::move(cfg));
            auto state_provider = std::make_shared<DigitalActuator>(peripheral->state_provider());

            sf.hotload_provider(DataProviders::FULL_SPECTRUM, std::move(state_provider));
            sf.hotload_peripheral(Peripherals::BROAD_SPECTRUM_RELAY, std::move(peripheral));
        }

        {
            const auto cfg = Relay::Config{.pin_cfg = DigitalOutput::Config{.pin = 13, .active_on_low = true},
                                           .backoff_time = time_ms(1000)};
            auto peripheral = std::make_unique<Relay>(std::move(cfg));
            auto state_provider = std::make_shared<DigitalActuator>(peripheral->state_provider());

            sf.hotload_provider(DataProviders::PUMP, std::move(state_provider));
            sf.hotload_peripheral(Peripherals::PUMP_RELAY, std::move(peripheral));
        }

        hws = sf.hardware_stack();
        hws->initialize();
    }

    {
        auto esc_cfg = EventSystem::Config{.events_count = static_cast<size_t>(Events::Size),
                                           .events_cap = 128,
                                           .handler_cap = 2,
                                           .delay_between_ticks = true,
                                           .min_delay_between_ticks = time_us{1},
                                           .max_delay_between_ticks = time_ms{1000}};
        auto prompt_cfg = kaskas::Prompt::Config{.io_buffer_size = 1024, .line_delimiters = "\r\n"};
        auto kk_cfg = KasKas::Config{.es_cfg = esc_cfg, .component_cap = 16, .prompt_cfg = prompt_cfg};
        kk = std::make_unique<KasKas>(hws, kk_cfg);
    }

    // todo: fully offload this to yaml file which is loaded in through pyserial API
    {
        using kaskas::component::ClimateControl;

        const auto ventilation_sample_interval = time_s(3);
        const auto heating_sample_interval = time_ms(1000);
        const auto max_heater_setpoint = 40.0; // maximum allowed heater setpoint

        auto cc_cfg =
            ClimateControl::Config{
                .hws_power_idx = ENUM_IDX(DataProviders::HEATING_POWER),
                .clock_idx = ENUM_IDX(DataProviders::CLOCK),

                .ventilation =
                    ClimateControl::Config::Ventilation{
                        .hws_climate_fan_idx = ENUM_IDX(DataProviders::CLIMATE_FAN),
                        .climate_humidity_idx = ENUM_IDX(DataProviders::CLIMATE_HUMIDITY),
                        // .climate_fan_pid = PID::Config{.tunings = PID::Tunings{.Kp = 34.57, .Ki = 2.71, .Kd = 0},
                        .climate_fan_pid =
                            // PID::Config{.tunings = PID::Tunings{.Kp = 37.13, .Ki = 0.217, .Kd = 0}, // 2.17
                        // PID::Config{.tunings = PID::Tunings{.Kp = 30.00, .Ki = 2.71, .Kd = 0.1}, // 2.17
                        PID::Config{.tunings = PID::Tunings{.Kp = 18.53, .Ki = 0.82, .Kd = 0.0}, // 2.17
                                    .output_lower_limit = 20,
                                    .output_upper_limit = 90,
                                    .sample_interval = ventilation_sample_interval,
                                    .direction = PID::Direction::FORWARD},
                        .minimal_duty_cycle = 0.21,
                        .heating_penality_weight = 1.0, // no ventilation until target heat has been reached
                        .schedule_cfg =
                            Schedule::Config{
                                .blocks = {Schedule::Block{.start = time_h(22), .duration = time_h(10), .value = 65.0},
                                           Schedule::Block{.start = time_h(8), .duration = time_h(14), .value = 75.0}}},
                        .check_interval = ventilation_sample_interval},

                .heating =
                    ClimateControl::Config::Heating{
                        .heating_element_fan_idx = ENUM_IDX(DataProviders::HEATING_SURFACE_FAN),
                        .heating_element_temp_sensor_idx = ENUM_IDX(DataProviders::HEATING_SURFACE_TEMP),
                        .climate_temp_sensor_idx = ENUM_IDX(DataProviders::CLIMATE_TEMP),
                        .ambient_temp_idx = ENUM_IDX(DataProviders::AMBIENT_TEMP),
                        .heater_cfg =
                            Heater::Config{
                                .pid_cfg =
                                    PID::Config{//
                                                .tunings =
                                                    PID::Tunings{.Kp = 62.590051, .Ki = 0.152824, .Kd = 0},
                                                .output_lower_limit = 0,
                                                .output_upper_limit = 255,
                                                .sample_interval = heating_sample_interval},
                                .max_heater_setpoint = max_heater_setpoint,
                                .heating_surface_temperature_idx = ENUM_IDX(DataProviders::HEATING_SURFACE_TEMP),
                                .heating_element_idx = ENUM_IDX(DataProviders::HEATING_ELEMENT),
                                .climate_trp_cfg = Heater::ThermalRunAway::Config{.stable_timewindow = time_m(30),
                                                                                  .heating_minimal_rising_c = 0.1,
                                                                                  .heating_minimal_dropping_c = 0.01,
                                                                                  .heating_timewindow = time_m(45)}},
                        .schedule_cfg =
                            Schedule::Config{
                                .blocks = {Schedule::Block{.start = time_h(0), .duration = time_h(24), .value = 16.0}}},
                        // Schedule::Config{
                        //     .blocks = {Schedule::Block{.start = time_h(0), .duration = time_h(7), .value = 16.0},
                        //                Schedule::Block{.start = time_h(7), .duration = time_h(2), .value = 18.0},
                        //                Schedule::Block{.start = time_h(9), .duration = time_h(1), .value = 20.0},
                        //                Schedule::Block{.start = time_h(10), .duration = time_h(1), .value = 22.0},
                        //                Schedule::Block{.start = time_h(11), .duration = time_h(1), .value = 24.0},
                        //                Schedule::Block{.start = time_h(12), .duration = time_h(8),.value = 27.0},
                        //                Schedule::Block{.start = time_h(20), .duration = time_h(2), .value = 24.0},
                        //                Schedule::Block{.start = time_h(22), .duration = time_h(2), .value = 16.0}}},
                        .check_interval = heating_sample_interval}};

        auto ventilation = std::make_unique<ClimateControl>(*hws, cc_cfg);
        kk->hotload_component(std::move(ventilation));
    }

    {
        using kaskas::component::Growlights;
        auto growlights_cfg =
            Growlights::Config{
                .redblue_spectrum_actuator_idx = ENUM_IDX(DataProviders::REDBLUE_SPECTRUM),
                .redblue_spectrum_schedule =
                    Schedule::Config{
                        .blocks =
                            {
                                Schedule::Block{.start = time_h(8), .duration = time_h(12), .value = LogicalState::ON},
                                Schedule::Block{
                                    .start = time_h(20), .duration = time_h(12), .value = LogicalState::OFF},
                            }},

                .full_spectrum_actuator_idx = ENUM_IDX(DataProviders::FULL_SPECTRUM),
                .full_spectrum_schedule =
                    Schedule::Config{
                        .blocks =
                            {
                                Schedule::Block{.start = time_h(6), .duration = time_h(16), .value = LogicalState::ON},
                                Schedule::Block{.start = time_h(22), .duration = time_h(8), .value = LogicalState::OFF},
                            }},
                .clock_idx = ENUM_IDX(DataProviders::CLOCK)};

        auto growlights = std::make_unique<Growlights>(*hws, growlights_cfg);
        kk->hotload_component(std::move(growlights));
    }

    {
        auto pump_cfg = Pump::Config{
            //
            .pump_actuator_idx = ENUM_IDX(DataProviders::PUMP),
            .interrupt_cfg =
                Interrupt::Config{
                    .pin = 2,
                    .mode = Interrupt::TriggerType::FALLING_EDGE,
                    .pull_up = false,
                },
            .ml_pulse_calibration = 25.8, // 25.8
            .reading_interval = time_ms(250), //
            .pump_timeout = time_s(10),
        };

        using kaskas::component::Fluidsystem;
        auto fluidsystem_cfg = Fluidsystem::Config{.pump_cfg = pump_cfg, //
                                                   .ground_moisture_sensor_idx = ENUM_IDX(DataProviders::SOIL_MOISTURE),
                                                   .clock_idx = ENUM_IDX(DataProviders::CLOCK),
                                                   .ground_moisture_target = 55, // target moisture percentage
                                                   .calibration_dosis_ml = 250,
                                                   .max_dosis_ml = 500,
                                                   .time_of_injection = time_h(6),
                                                   .delay_before_effect_evaluation = time_h(2)};
        auto fluidsystem = std::make_unique<Fluidsystem>(*hws, fluidsystem_cfg);
        kk->hotload_component(std::move(fluidsystem));
    }

    {
        using kaskas::component::UI;
        auto ui_cfg =
            UI::Config{//
                       .signaltower_cfg =
                           Signaltower::Config{
                               .pin_red = DigitalOutput(DigitalOutput::Config{.pin = 10, .active_on_low = false}),
                               .pin_yellow = DigitalOutput(DigitalOutput::Config{.pin = 9, .active_on_low = false}),
                               .pin_green = DigitalOutput(DigitalOutput::Config{.pin = 8, .active_on_low = false}),
                           },
                       .userbutton_cfg = DigitalInput::Config{.pin = PC13, .pull_up = false},
                       .watchdog_interval = time_s(1),
                       .prompt_interval = time_ms(25)};
        auto ui = std::make_unique<UI>(*hws, ui_cfg);
        ui->hotload_prompt(kk->prompt());
        kk->hotload_component(std::move(ui));
    }

    {
        using kaskas::component::Hardware;
        auto hardware_cfg = Hardware::Config{};

        auto hardware = std::make_unique<Hardware>(*hws, hardware_cfg);
        kk->hotload_component(std::move(hardware));
    }

    {
        static constexpr auto datasources = {DataProviders::CLIMATE_TEMP,
                                             DataProviders::HEATING_SURFACE_TEMP,
                                             DataProviders::AMBIENT_TEMP,
                                             DataProviders::HEATING_SETPOINT,
                                             DataProviders::HEATING_ELEMENT,
                                             DataProviders::CLIMATE_HUMIDITY,
                                             DataProviders::CLIMATE_HUMIDITY_SETPOINT,
                                             DataProviders::CLIMATE_FAN,
                                             DataProviders::SOIL_MOISTURE,
                                             DataProviders::SOIL_MOISTURE_SETPOINT,
                                             DataProviders::FLUID_INJECTED,
                                             DataProviders::FLUID_INJECTED_CUMULATIVE,
                                             DataProviders::FLUID_EFFECT};

        using kaskas::component::DataAcquisition;
        auto cfg = DataAcquisition::Config{.initial_warm_up_time = time_s(30), .active_dataproviders = datasources};

        auto ctrl = std::make_unique<DataAcquisition>(*hws, cfg);
        kk->hotload_component(std::move(ctrl));
    }
    kk->initialize();
}
void loop() {
    if (kk == nullptr) {
        DBG("loop");
        HAL::delay(time_ms(1000));
    } else {
        kk->loop();
    }
}

#    if defined(UNITTEST) || defined(NATIVE)
int main(int argc, char** argv) {
    setup();
    while (true) {
        loop();
    }
    return 0;
}
#    endif

#endif
