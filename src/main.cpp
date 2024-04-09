#include "Actuator.hpp"
#include "Clock.hpp"
#include "Sensor.hpp"

#ifdef ARDUINO
#    include <Arduino.h>
#endif

int setup() { return 0; }
int loop() { return 0; }

int main() {
    int rval = 0;

    rval = setup();
    if (rval != 0) {
        return rval;
    }
    while (rval == 0) {
        rval = loop();
    }
    return rval;
}
