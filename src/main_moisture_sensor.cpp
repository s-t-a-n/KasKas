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
//                                        .delay_between_ticks = true,
//                                        .min_delay_between_ticks = 1,
//                                        .max_delay_between_ticks = 1000};
//
//     auto growlights_cfg = Growlights::Config{
//         .violet_spectrum = Relay(Relay::Config{
//             .pin_cfg = DigitalOutput::Config{.pin = 34, // 50 for led green
//                                              .active_on_low = true}, //
//             .backoff_time = 1000 //
//         }),
//         .broad_spectrum = Relay(Relay::Config{
//             .pin_cfg = DigitalOutput::Config{.pin = 35, // 51 for led yellow
//                                              .active_on_low = true}, //
//             .backoff_time = 1000 //
//         }), //
//         .starting_hour = 21, //
//         .duration_hours = 1 //
//     };
//
//     using GroundMoistureSensor = Fluidsystem::GroundMoistureSensor;
//     using GroundMoistureSensorFilter = Fluidsystem::GroundMoistureSensorFilter;
//     using GroundMoistureSensorConfig = Fluidsystem::GroundMoistureSensor::Config;
//     auto fluidsystem_cfg = Fluidsystem::Config{
//         .pump = Relay(Relay::Config{
//             .pin_cfg = DigitalOutput::Config{.pin = 33, // 52 for red led
//                                              .active_on_low = true}, //
//             .backoff_time = 1000 //
//         }), //
//         .ground_moisture_sensor = GroundMoistureSensor(GroundMoistureSensorConfig{
//             .sensor = AnalogueInput(AnalogueInput::Config{.pin = A1, .pull_up = false, .resolution = 10}), //
//             .filter = GroundMoistureSensorFilter(GroundMoistureSensorFilter::Config{.K = 100, .invert = true})}),
//         .ground_moisture_threshold = 0.5,
//         .interrupt = Interrupt(Interrupt::Config{
//             .pin = 2, //
//             .mode = Interrupt::Mode::FALLING, //
//             .pull_up = true, //
//         }),
//         .dosis_ml = 500,
//         .pump_check_interval = time_ms(250),
//         .check_interval_hours = 3,
//         .ml_calibration_factor = 25.8,
//         .timeout = time_s(120),
//         .timeout_to_half_point = time_s(60),
//     };
//
//     auto kk_cfg = KasKas::Config{
//         .esc_cfg = esc_cfg, //
//         .growlights_cfg = growlights_cfg, //
//         .fluidsystem_cfg = fluidsystem_cfg,
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
