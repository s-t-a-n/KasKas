#include <spine/core/debugging.hpp>
#include <spine/platform/hal.hpp>

void setup() {
    HAL::initialize(ArduinoConfig{.baudrate = 9600});
    HAL::println("wake up");
}
void loop() {
    HAL::println("loop");
    HAL::delay_ms(1000);
}

#ifdef DEBUG_KASKAS
int main(int argc, char** argv) {
    setup();
    while (true) {
        loop();
    }
    return 0;
}
#endif
