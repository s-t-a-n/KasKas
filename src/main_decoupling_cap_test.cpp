// #include <spine/core/debugging.hpp>
// #include <spine/platform/hal.hpp>
//
// DigitalOutput* pin = nullptr;
//
// void setup() {
//     HAL::initialize(ArduinoConfig{.baudrate = 115200});
//     HAL::println("wake up");
//     pin = new DigitalOutput(DigitalOutput::Config{.pin = 34, .active_on_low = true});
//     pin->initialize();
//     HAL::delay_ms(2000);
//     pin->set_state(true);
//     HAL::delay_ms(3000);
//     pin->set_state(false);
// }
// void loop() {
//     HAL::println("loop");
//     HAL::delay_ms(1000);
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
