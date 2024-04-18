#if defined(ARDUINO) && defined(EMBEDDED)
#    include <Arduino.h>
#else
#    include <ArduinoFake.h>
#endif

// macro
#include <spine/platform/implementations/arduino.hpp>
#include <spine/platform/platform.hpp>
using namespace spn::platform;
using HAL = Platform<Arduino>;
// end macro

void setup() {
    // Serial.begin(9600);
    // Serial.println("Wake up");
    HAL::initialize();
    HAL::print("hello");
    auto pin = HAL::DigitalPin({.pin = 50, .active_on_low = false});
    pin.set_state = (ON);
}
void loop() {
    Serial.println("loop");
    delay(1000);
}

#ifdef DEBUG
int main(int argc, char** argv) {
    setup();
    while (true) {
        loop();
    }
    return 0;
}
#endif
