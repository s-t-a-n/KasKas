#include "kaskas/KasKas.hpp"
#include "kaskas/io/implementations/DS18B20_Temp_Probe.hpp"

#include <spine/controller/pid.hpp>
#include <spine/core/debugging.hpp>
#include <spine/filter/implementations/ewma.hpp>

using spn::filter::EWMA;

using KasKas = kaskas::KasKas;

static KasKas* kk;

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
using spn::controller::SRLatch;
using spn::core::time::Schedule;

// volatile uint8_t interruptor = 0;
// DigitalInput ub();

void setup() {
    HAL::initialize(HAL::Config{.baudrate = 115200});
    HAL::println("Wake up");

    // PID p(PID::Config{.tunings = PID::Tunings{.Kp = 60.841, .Ki = 0.376, .Kd = 0.1},
    //                   .output_lower_limit = 0,
    //                   .output_upper_limit = 255,
    //                   .sample_interval = time_ms(1000)});
    // p.initialize();
    // p.set_target_setpoint(22.25);
    // p.new_reading(19.88);
    //
    // while (true) {
    //     p.new_reading(20.187500);
    //     DBGF("response: %f", p.response());
    //     HAL::delay(time_s(1));
    // }
    // return;

    // Heater h(Heater::Config{.pid_cfg = PID::Config{.tunings = PID::Tunings{.Kp = 60.841, .Ki = 0.376, .Kd = 0.1},
    //                                                .output_lower_limit = 0,
    //                                                .output_upper_limit = 255,
    //                                                .sample_interval = time_ms(1000)},
    //                         .heater_cfg = AnalogueOutput::Config{.pin = 6, .active_on_low = true},
    //                         .tempprobe_cfg = DS18B20TempProbe::Config{.pin = 3},
    //                         .tempprobe_filter_cfg = BandPass::Config{
    //                             .mode = BandPass::Mode::RELATIVE, .mantissa = 1, .decades = 0.01, .offset = 0}});
    // h.initialize();
    // h.set_setpoint(5.0);
    // h.update();
    // return;

    // AnalogueOutput a(AnalogueOutput::Config{.pin = 6, .active_on_low = true});
    // a.initialize();
    // while (true) {
    //     a.fade_to(1.0);
    //     HAL::delay(time_s(1));
    //     a.fade_to(0.0);
    //     HAL::delay(time_s(1));
    // }
    // return;

    // const auto callback = []() { interruptor++; };
    //
    // Interrupt i(
    //     Interrupt::Config{.pin = PC13, .mode = Interrupt::TriggerType::RISING_AND_FALLING_EDGE, .pull_up = true});
    // i.initialize();
    // i.attach_interrupt(callback);

    // while (true) {
    //     time_us microseconds = HAL::micros();
    //
    //     const auto interval = time_us(1);
    //     while (HAL::micros() - microseconds < time_s(1)) {
    //         HAL::delay_us(interval);
    //     }
    //     DBGF("hello loop: us: %i", microseconds.raw<>())
    // }

    // DigitalOutput led_blue(DigitalOutput::Config{.pin = PB7, .active_on_low = false});
    // led_blue.initialize();
    // led_blue.set_state(LogicalState::ON);
    //
    // DigitalOutput led_red(DigitalOutput::Config{.pin = PB14, .active_on_low = false});
    // led_red.initialize();
    // led_red.set_state(LogicalState::ON);

    // ub.initialize();

    // const auto orig = time_ms(500000000000);
    // auto t = time_us(orig);
    // assert(500000000000000 * 1000 == 500000000000000000);
    // assert(time_ms(t) == orig);
    // assert(t.raw<>() == orig.raw<>() * 1000);
    //
    // time_ms a, b, c;
    // a = b = c = time_ms{};
    // a.operator+=(time_ms(1));
    // assert(a == time_ms(1));
    // assert(b == time_ms(0));
    // assert(a != b);
    // assert(b == c);
    // return;

    {
        auto esc_cfg = EventSystem::Config{.events_count = static_cast<size_t>(Events::Size),
                                           .events_cap = 20,
                                           .handler_cap = 2,
                                           .delay_between_ticks = true,
                                           .min_delay_between_ticks = time_ms{1},
                                           .max_delay_between_ticks = time_ms{1000}};
        auto kk_cfg = KasKas::Config{.esc_cfg = esc_cfg, .component_cap = 16};
        kk = new KasKas(kk_cfg);
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