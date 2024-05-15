// #include "kaskas/KasKas.hpp"
// #include "kaskas/io/implementations/DS18B20_Temp_Probe.hpp"
//
// #include <spine/controller/pid.hpp>
// #include <spine/core/debugging.hpp>
// #include <spine/filter/implementations/ewma.hpp>
//
// using spn::filter::EWMA;
//
// using KasKas = kaskas::KasKas;
//
// static KasKas* kk;
//
// using spn::controller::Pid;
// Pid* pid = nullptr;
//
// using kaskas::io::DS18B20TempProbe;
// DS18B20TempProbe* probe = nullptr;
//
// AnalogueOutput* heater = nullptr;
//
// void setup() {
//     HAL::initialize(HAL::Config{.baudrate = 115200});
//     HAL::println("Wake up");
//
//     {
//         auto esc_cfg = EventSystem::Config{.events_count = static_cast<size_t>(Events::Size),
//                                            .events_cap = 128,
//                                            .handler_cap = 2,
//                                            .delay_between_ticks = true,
//                                            .min_delay_between_ticks = time_ms{1},
//                                            .max_delay_between_ticks = time_ms{1000}};
//         auto kk_cfg = KasKas::Config{.esc_cfg = esc_cfg, .component_cap = 16};
//         kk = new KasKas(kk_cfg);
//     }
//
//     {
//         auto ventilation_cfg =
//             Ventilation::Config{//
//                                 .fan_pwm_cfg = AnalogueOutput::Config{.pin = 5, .active_on_low = true},
//                                 .heater_pwm_cfg = AnalogueOutput::Config{.pin = 6, .active_on_low = false},
//                                 .airconditioning_power_cfg =
//                                     Relay::Config{.pin_cfg = DigitalOutput::Config{.pin = 32, .active_on_low = true},
//                                                   .backoff_time = time_s(10)},
//                                 .on_time = time_m(30),
//
//                                 .off_time = time_h(3)};
//         auto ventilation = std::make_unique<Ventilation>(ventilation_cfg);
//         kk->hotload_component(std::move(ventilation));
//     }
//
//     {
//         auto growlights_cfg = Growlights::Config{
//             .violet_spectrum_cfg =
//                 Relay::Config{
//                     .pin_cfg = DigitalOutput::Config{.pin = 34, // 50 for led green
//                                                      .active_on_low = true}, //
//                     .backoff_time = time_ms(1000) //
//                 },
//             .broad_spectrum_cfg =
//                 Relay::Config{
//                     .pin_cfg = DigitalOutput::Config{.pin = 35, // 51 for led yellow
//                                                      .active_on_low = true}, //
//                     .backoff_time = time_ms(1000) //
//                 }, //
//             .starting_hour = time_h{6}, //
//             .duration_hours = time_h{16} //
//         };
//
//         auto growlights = std::make_unique<Growlights>(growlights_cfg);
//         kk->hotload_component(std::move(growlights));
//     }
//
//     {
//         auto pump_cfg = Pump::Config{
//             //
//             .pump_relay_cfg =
//                 Relay::Config{
//                     .pin_cfg = DigitalOutput::Config{.pin = 33, // 52 for red led
//                                                      .active_on_low = true}, //
//                     .backoff_time = time_ms(1000) //
//                 }, //
//             .interrupt_cfg =
//                 Interrupt::Config{
//                     .pin = 2, //
//                     .mode = Interrupt::Mode::FALLING_EDGE, //
//                     .pull_up = false, //
//                 },
//             .ml_pulse_calibration = 25.8, //
//             .reading_interval = time_ms(250), //
//             .pump_timeout = time_s(10),
//         };
//
//         using GroundMoistureSensorFilter = Fluidsystem::GroundMoistureSensorFilter;
//         using GroundMoistureSensorConfig = Fluidsystem::GroundMoistureSensor::Config;
//         auto fluidsystem_cfg = Fluidsystem::Config{
//             .pump_cfg = pump_cfg, //
//             .ground_moisture_sensor_cfg =
//                 GroundMoistureSensorConfig{.sensor_cfg =
//                                                AnalogueInput::Config{.pin = A1, .pull_up = false, .resolution = 10},
//                                            //
//                                            .filter_cfg = GroundMoistureSensorFilter::Config{.K = 100, .invert =
//                                            false}},
//             .ground_moisture_threshold = 0.5,
//
//             .inject_dosis_ml = 250,
//             .inject_check_interval = time_h(24),
//         };
//         auto fluidsystem = std::make_unique<Fluidsystem>(fluidsystem_cfg);
//         kk->hotload_component(std::move(fluidsystem));
//     }
//
//     {
//         auto ui_cfg = UI::Config{
//             //
//             .signaltower_cfg =
//                 Signaltower::Config{
//                     //
//                     .pin_red = DigitalOutput(DigitalOutput::Config{.pin = 52, .active_on_low = false}), //
//                     .pin_yellow = DigitalOutput(DigitalOutput::Config{.pin = 51, .active_on_low = false}), //
//                     .pin_green = DigitalOutput(DigitalOutput::Config{.pin = 50, .active_on_low = false}), //
//                 },
//
//         };
//         auto ui = std::make_unique<UI>(ui_cfg);
//         kk->hotload_component(std::move(ui));
//     }
//
//     kk->initialize();
// }
// void loop() {
//     // DBG("loop");
//     HAL::delay(time_ms(1000));
//     kk->loop();
// }
//
// #if defined(UNITTEST) || defined(NATIVE)
// int main(int argc, char** argv) {
//     setup();
//     while (true) {
//         loop();
//     }
//     return 0;
// }
// #endif