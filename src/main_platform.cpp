// #include <spine/platform/hal.hpp>

// void setup() {
//     HAL::initialize(ArduinoConfig({.baudrate = 9600}));
//     HAL::println("Wake up");

//     auto outputpin = DigitalOutput(DigitalOutput::Config({.pin = 50, .active_on_low = false}));
//     outputpin.initialize();
//     outputpin.set_state(ON);

//     auto inputpin = DigitalInput(DigitalInput::Config({.pin = 50, .pull_up = false}));
//     inputpin.initialize();

//     HAL::println("PRESS");
//     HAL::delay_ms(1000);
//     HAL::println(inputpin.state());

//     HAL::println("End of setup");
// }
// void loop() {
//     HAL::println("loop");
//     HAL::delay_ms(1000);
// }

// #ifdef DEBUG
// int main(int argc, char** argv) {
//     setup();
//     while (true) {
//         loop();
//     }
//     return 0;
// }
// #endif
