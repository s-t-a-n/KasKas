#if defined(ARDUINO)
#    include <Arduino.h>
#endif

#include "KasKas.hpp"


static auto KASKAS = KasKas(KasKas::Config());

void setup() {
    if(KASKAS.init() != 0 ) {
        block();
    }
}

void loop() {
   if(KASKAS.loop() != 0) {
        block();
   };
}

void block() {
    while(true);
}

// int main() {
//     setup();
//     while (HAS_ERROR == false) { loop(); }
//     return ERROR;
// }
