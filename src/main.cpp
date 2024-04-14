#if defined(ARDUINO)
#    include <Arduino.h>
#endif

#include "KasKas.hpp"


static auto KASKAS = KasKas(KasKas::Config());

void block() {
    while(true);
}

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


// int main() {
//     setup();
//     while (HAS_ERROR == false) { loop(); }
//     return ERROR;
// }
