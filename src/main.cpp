// #include "kaskas/KasKas.hpp"
//
// #include <spine/core/debugging.hpp>
//
// using GroundMoistureSensor = Fluidsystem::GroundMoistureSensor;
// using GroundMoistureSensorFilter = Fluidsystem::GroundMoistureSensorFilter;
// using GroundMoistureSensorConfig = Fluidsystem::GroundMoistureSensor::Config;
//
// static GroundMoistureSensor* sensor;
// static AnalogueInput* sensor_source;
//
// void setup() {
//     HAL::initialize(HAL::Config{.baudrate = 115200});
//     HAL::println("Wake up");
//
//     sensor_source = new AnalogueInput(AnalogueInput::Config{.pin = A1, .pull_up = false, .resolution = 10});
//     sensor = new GroundMoistureSensor(GroundMoistureSensorConfig{
//         .sensor = *sensor_source, //
//         .filter = GroundMoistureSensorFilter(GroundMoistureSensorFilter::Config{.K = 10, .invert = true})});
//
//     sensor->update();
//
//     //    kk = new KasKas(kk_cfg);
//     //    kk->setup();
// }
// void loop() {
//     //    DBG("loop");
//     char formatted_msg[256];
//     snprintf(formatted_msg, sizeof(formatted_msg), "Raw: %f, filtered: %f\n", sensor_source->read() * 1024,
//              sensor->value());
//     HAL::println(formatted_msg);
//     sensor->update();
//
//     HAL::delay_ms(1000);
//     //    kk->loop();
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
