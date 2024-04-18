// #if defined(ARDUINO) && defined(EMBEDDED)
// #    include <Arduino.h>
// #else
// #    include <ArduinoFake.h>
// #endif

// // #include "KasKas.hpp"
// #include "kaskas/components/Signaltower.hpp"

// // #include <spine/core/assert.hpp>
// // #include <spine/core/exception.hpp>
// // #include <spine/core/standard.hpp>
// // #include <spine/core/system.hpp>

// #include <kaskas/events.hpp>
// #include <spine/core/eventsystem.hpp>

// using namespace spn::core;

// // static auto KASKAS = KasKas(KasKas::Config());

// Signaltower* S = nullptr;

// using EventSystemControllerT = EventSystemController<Events>;
// using EventSystemT = EventSystem<Events>;

// Event* OutOfWater = nullptr;
// Event* WateringTime = nullptr;
// Event* WaterParty = nullptr;

// class HydroEventHandler : public EventHandler {
// public:
//     HydroEventHandler(const char* name, EventSystemT& evsys) : EventHandler(name), _evsys(evsys){};

//     virtual void handle_event(Event& event) {
//         // Serial.println("HydroEventHandler: event is being handled!");
//         switch (static_cast<Events>(event.id())) {
//         case Events::OutOfWater: S->signal(Signaltower::State::Yellow); break;
//         case Events::WateringTime: S->signal(Signaltower::State::Red); break;
//         case Events::WaterParty:
//             // Serial.println("waterparty");
//             S->blink();
//             _evsys.schedule(WaterParty);
//             break;
//         default: break;
//         }
//     }

// private:
//     EventSystemT& _evsys;
// };

// EventSystemControllerT* esc = nullptr;
// HydroEventHandler* handler = nullptr;

// void setup() {
//     Serial.begin(9600);
//     Serial.println("Wake up");
//     // if (KASKAS.init() != 0) {
//     //     halt("KasKas failed to initialize.");
//     // }
//     auto cfg = Signaltower::Config(
//         {.pin_red = DigitalOutputPin(52), .pin_yellow = DigitalOutputPin(51), .pin_green = DigitalOutputPin(50)});
//     S = new Signaltower(cfg);
//     S->initialize();

//     auto sc_cfg = EventSystemControllerT::Config({.events_cap = 128,
//                                                   .delay_between_ticks = true,
//                                                   .min_delay_between_ticks = 1,
//                                                   .max_delay_between_ticks = 1000});
//     esc = new EventSystemControllerT(sc_cfg);

//     auto s = esc->eventSystem();
//     handler = new HydroEventHandler("HydroEventHandler", *s);
//     s->attach(EventId(Events::OutOfWater), handler);
//     s->attach(EventId(Events::WateringTime), handler);
//     s->attach(EventId(Events::WaterParty), handler);

//     OutOfWater = new Event(EventId(Events::OutOfWater), Event::Data(), time_ms(100)); // yellow
//     WateringTime = new Event(EventId(Events::WateringTime), Event::Data(), time_s(3)); // red
//     WaterParty = new Event(EventId(Events::WaterParty), Event::Data(), time_ms(500)); // blink

//     // s->schedule(WaterParty);
//     s->schedule(OutOfWater);
//     s->schedule(WateringTime);

//     Serial.println(s->to_string());
// }
// void loop() {
//     // if (KASKAS.loop() != 0) {
//     //    halt("KasKas left loop.");
//     // };
//     // S->signal(Signaltower::State::Green);
//     // delay(1000);
//     // S->signal(Signaltower::State::Yellow);
//     // delay(1000);
//     // S->signal(Signaltower::State::Red);
//     // delay(1000);
//     Serial.println("loop");
//     esc->loop();
// }

// // int main() {
// //     setup();
// //     while (HAS_ERROR == false) { loop(); }
// //     return ERROR;
// // }
