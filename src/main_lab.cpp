// #include "kaskas/KasKas.hpp"
//
// #include <spine/core/debugging.hpp>
//
// static KasKas* kk;
//
// void setup() {
//     HAL::initialize(HAL::Config{.baudrate = 115200});
//     HAL::println("Wake up");
//
//     auto esc_cfg = EventSystem::Config{.events_count = static_cast<size_t>(Events::Size),
//                                        .events_cap = 128,
//                                        .handler_cap = 2,
//                                        .delay_between_ticks = true,
//                                        .min_delay_between_ticks = time_ms(1),
//                                        .max_delay_between_ticks = time_ms(1000)};
//
//     auto growlights_cfg = Growlights::Config{
//         .violet_spectrum = Relay(Relay::Config{
//             .pin_cfg = DigitalOutput::Config{.pin = 34, // 50 for led green
//                                              .active_on_low = true}, //
//             .backoff_time = time_ms(1000) //
//         }),
//         .broad_spectrum = Relay(Relay::Config{
//             .pin_cfg = DigitalOutput::Config{.pin = 35, // 51 for led yellow
//                                              .active_on_low = true}, //
//             .backoff_time = time_ms(1000) //
//         }), //
//         .starting_hour = 6, //
//         .duration_hours = 18 //
//     };
//
//     auto pump_cfg = Pump::Config{
//         //
//         .pump_relay_cfg =
//             Relay::Config{
//                 .pin_cfg = DigitalOutput::Config{.pin = 33, // 52 for red led
//                                                  .active_on_low = true}, //
//                 .backoff_time = time_ms(1000) //
//             }, //
//         .interrupt_cfg =
//             Interrupt::Config{
//                 .pin = 2, //
//                 .mode = Interrupt::Mode::FALLING, //
//                 .pull_up = true, //
//             },
//         .ml_calibration_factor = 25.8,
//         .reading_interval = time_ms(250),
//         .pump_timeout = time_s(10),
//     };
//
//     //    using GroundMoistureSensor = Fluidsystem::GroundMoistureSensor;
//     using GroundMoistureSensorFilter = Fluidsystem::GroundMoistureSensorFilter;
//     using GroundMoistureSensorConfig = Fluidsystem::GroundMoistureSensor::Config;
//     auto fluidsystem_cfg = Fluidsystem::Config{
//         .pump_cfg = pump_cfg, //
//         .ground_moisture_sensor_cfg =
//             GroundMoistureSensorConfig{.sensor_cfg =
//                                            AnalogueInput::Config{.pin = A1, .pull_up = false, .resolution = 10}, //
//                                        .filter_cfg = GroundMoistureSensorFilter::Config{.K = 100, .invert = true}},
//         .ground_moisture_threshold = 0.5,
//
//         .inject_dosis_ml = 250,
//         .inject_check_interval = time_h(3),
//     };
//
//     auto ui_cfg = UI::Config{
//         //
//         .signaltower_cfg =
//             Signaltower::Config{
//                 //
//                 .pin_red = DigitalOutput(DigitalOutput::Config{.pin = 52, .active_on_low = false}), //
//                 .pin_yellow = DigitalOutput(DigitalOutput::Config{.pin = 51, .active_on_low = false}), //
//                 .pin_green = DigitalOutput(DigitalOutput::Config{.pin = 50, .active_on_low = false}), //
//             },
//
//     };
//
//     auto kk_cfg = KasKas::Config{
//         .esc_cfg = esc_cfg, //
//         .growlights_cfg = growlights_cfg, //
//         .fluidsystem_cfg = fluidsystem_cfg,
//         .ui_cfg = ui_cfg,
//     };
//
//     kk = new KasKas(kk_cfg);
//     kk->setup();
// }
// void loop() {
//     //    DBG("loop");
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
