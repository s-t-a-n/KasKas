// #if defined(ARDUINO) && defined(EMBEDDED)
// #    include <Arduino.h>
// #else
// #    include <ArduinoFake.h>
// #endif

// #include "kaskas/KasKas.hpp"

// static KasKas* kk;

// using spn::core::EventStore;
// using spn::core::Pipeline;

// class EventSystemController2 {
// public:
// public:
//     EventSystemController2(const EventSystemController::Config& cfg) //
//         : _cfg(cfg), //
//           _evsys(EventSystem(nullptr, cfg.events_count)), //
//           _pipeline(Pipeline(cfg.events_cap)), //
//           _store(EventStore(cfg.events_cap)) {
//         Serial.print("hallo maar echt");
//         delay(1000);
//     }

// private:
//     EventSystemController::Config _cfg;
//     EventSystem _evsys;
//     Pipeline _pipeline; // stores all events queued for firing
//     EventStore _store; // maintains event's lifetime in memory
// };

// void setup() {
//     Serial.begin(9600);
//     Serial.println("Wake up");

//     auto esc_cfg = EventSystemController::Config{.events_count = static_cast<size_t>(Events::Size),
//                                                  .events_cap = 2,
//                                                  .delay_between_ticks = true,
//                                                  .min_delay_between_ticks = 1,
//                                                  .max_delay_between_ticks = 1000};

//     auto growlights_cfg = Growlights::Config{.violet_spectrum = Relay(Relay::Config{
//                                                  .pin_cfg = DigitalOutputPin::Config{.pin = 50, //
//                                                                                      .active_on_low = false}, //
//                                                  .backoff_time = 1000 //
//                                              }),
//                                              .broad_spectrum = Relay(Relay::Config{
//                                                  .pin_cfg = DigitalOutputPin::Config{.pin = 51, //
//                                                                                      .active_on_low = false}, //
//                                                  .backoff_time = 1000 //
//                                              }),
//                                              .back_off_time = time_ms(1000)};

//     auto kk_cfg = KasKas::Config{
//         .esc_cfg = esc_cfg, //
//         .growlights_cfg = growlights_cfg //
//     };

//     Serial.println("generasting kaskas");
//     kk = new KasKas(kk_cfg);
//     Serial.println("handof to kaskas");
//     kk->setup();
// }
// void loop() {
//     Serial.println("loop");
//     kk->loop();
// }

// #ifdef DEBUG_KASKAS
// int main(int argc, char** argv) {
//     setup();
//     while (true) {
//         loop();
//     }
//     return 0;
// }
// #endif
