#if defined(ARDUINO)
#    include <Arduino.h>
#else
#    include <ArduinoFake.h>
#endif

#include "KasKas.hpp"

#include <spine/core/assert.hpp>
#include <spine/core/exception.hpp>
#include <spine/core/standard.hpp>
#include <spine/core/system.hpp>

namespace {

using namespace spn::core;

static auto KASKAS = KasKas(KasKas::Config());

void setup() {
    if (KASKAS.init() != 0) {
        halt("KasKas failed to initialize.");
    }
}

void loop() {
    if (KASKAS.loop() != 0) {
       halt("KasKas left loop.");
    };
}

} // namespace

// int main() {
//     setup();
//     while (HAS_ERROR == false) { loop(); }
//     return ERROR;
// }
