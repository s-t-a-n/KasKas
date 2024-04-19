#include "kaskas/KasKas.hpp"

#include <spine/core/debugging.hpp>

static KasKas* kk;

void setup() {
    HAL::initialize(HAL::Config{.baudrate = 9600});
    HAL::println("Wake up");

    auto esc_cfg = EventSystem::Config{.events_count = static_cast<size_t>(Events::Size),
                                       .events_cap = 128,
                                       .delay_between_ticks = true,
                                       .min_delay_between_ticks = 1,
                                       .max_delay_between_ticks = 1000};

    auto growlights_cfg = Growlights::Config{
        .violet_spectrum = Relay(Relay::Config{
            .pin_cfg = DigitalOutput::Config{.pin = 50, //
                                             .active_on_low = false}, //
            .backoff_time = 1000 //
        }),
        .broad_spectrum = Relay(Relay::Config{
            .pin_cfg = DigitalOutput::Config{.pin = 51, //
                                             .active_on_low = false}, //
            .backoff_time = 1000 //
        }), //
        .starting_hour = 21, //
        .duration_hours = 11 //
    };

    auto kk_cfg = KasKas::Config{
        .esc_cfg = esc_cfg, //
        .growlights_cfg = growlights_cfg //
    };

    kk = new KasKas(kk_cfg);
    kk->setup();
}
void loop() {
    //    DBG("loop");
    kk->loop();
}

#if defined(UNITTEST) || defined(NATIVE)
int main(int argc, char** argv) {
    setup();
    while (true) {
        loop();
    }
    return 0;
}
#endif
