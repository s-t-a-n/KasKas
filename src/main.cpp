#ifdef ARDUINO
#    include <Arduino.h>
#endif


#include "KasKas.hpp"

auto kaskas = KasKas(KasKas::Config());

int setup() {
    return(kaskas.init());
}
int loop() {
    return(kaskas.loop());
}

int main() {
    int rval = 0;

    // rval = setup();
    if ((rval = setup()) != 0) {
        return rval;
    }
    while (rval == 0) {
        rval = loop();
    }
    return rval;
}
