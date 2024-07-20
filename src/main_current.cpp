// //
// // Created by stan on 9-5-24.
// //
// #if !defined(UNITTEST)
//
// #    include <Arduino_Helpers.h>
//
// #    if not defined(DEBUG_SPINE)
// #        define DEBUG_SPINE
// #    endif
//
// #    include "kaskas/components/relay.hpp"
// #    include "spine/core/debugging.hpp"
// #    include "spine/filter/implementations/ewma.hpp"
// #    include "spine/platform/hal.hpp"
//
// // using namespace spn::transport;
//
// // using spn::structure::Pool;
//
// // const auto pin1 = PA_8;
// // const auto pin2 = PB_10;
// // const auto apin1 = PA_4;
//
// #    include <DS18B20.h>
//
// double baseline = 0;
//
// using spn::filter::EWMA;
//
// uint8_t power_relay_pin = D4;
// uint8_t fan_relay_pin = D7;
// uint8_t heater_current_pin = D6;
// uint8_t temp_probe_pin = D5;
// Relay* power;
// Relay* fan;
// AnalogueOutput* heater;
//
// DS18B20* ds;
//
// void setup() {
//     HAL::initialize(ArduinoConfig{.baudrate = 115200});
//     HAL::println("wake up");
//
//     ds = new DS18B20(temp_probe_pin);
//     return;
//
//     power = new Relay(Relay::Config{.pin_cfg = DigitalOutput::Config{.pin = power_relay_pin, .active_on_low = false},
//                                     .backoff_time = time_ms(1000)});
//     fan = new Relay(Relay::Config{.pin_cfg = DigitalOutput::Config{.pin = fan_relay_pin, .active_on_low = true},
//                                   .backoff_time = time_ms(1000)});
//     heater = new AnalogueOutput(AnalogueOutput::Config{.pin = heater_current_pin, .active_on_low = true});
//     heater->initialize();
//     power->initialize();
//     fan->initialize();
//
//     HAL::delay_ms(3000);
//     power->set_state(DigitalState::ON);
//
//     HAL::delay_ms(3000);
//     fan->set_state(DigitalState::ON);
//     HAL::delay_ms(3000);
//     fan->set_state(DigitalState::OFF);
//
//     HAL::delay_ms(1000);
// }
//
// void loop() {
//     // HAL::println("loop");
//     HAL::delay_ms(1000);
//
//     Serial.println(ds->getTempC());
// }
//
// #    if defined(UNITTEST) || defined(NATIVE)
// int main(int argc, char** argv) {
//     setup();
//     while (true) {
//         loop();
//     }
//     return 0;
// }
// #    endif
//
// #endif

//
// SECOND
//

// #include "kaskas/components/relay.hpp"
//
// #include <spine/core/debugging.hpp>
// #include <spine/filter/implementations/ewma.hpp>
//
// void setup() {
//     HAL::initialize(HAL::Config{.baudrate = 115200});
//     HAL::println("Wake up");
//
//     Relay power(Relay::Config{.pin_cfg = DigitalOutput::Config{.pin = PA_10, .active_on_low = false},
//                               .backoff_time = time_ms(1000)});
//     power.initialize();
//     Relay fan(Relay::Config{.pin_cfg = DigitalOutput::Config{.pin = PB_3, .active_on_low = true},
//                             .backoff_time = time_ms(1000)});
//     fan.initialize();
//     HAL::delay_ms(3000);
//     power.set_state(DigitalState::ON);
//
//     // HAL::delay_ms(3000);
//     // fan.set_state(DigitalState::ON);
//     // HAL::delay_ms(3000);
//     // fan.set_state(DigitalState::OFF);
//
//     HAL::delay_ms(3000);
//     AnalogueOutput heater(AnalogueOutput::Config{.pin = PA_8, .active_on_low = true});
//     heater.initialize();
//
//     auto heater_current_pin = PA_1;
//     pinMode(heater_current_pin, INPUT_PULLUP);
//     HAL::delay_ms(100);
//     auto baseline = 0;
//     const auto baseline_repeat = 100;
//
//     HAL::delay_ms(5000);
//     for (int i = 0; i < baseline_repeat; i++) {
//         baseline += analogRead(heater_current_pin);
//         HAL::delay_ms(1);
//     }
//     baseline /= baseline_repeat;
//     DBG("Baseline %u", baseline);
//
//     // power.set_state(DigitalState::OFF);
//     // return;
//
//     constexpr auto vRef = 4.92;
//     // constexpr auto vRef = 3.3;
//     constexpr auto adc_base = 1024.0;
//     constexpr auto mV_per_step = vRef * 1000.0 / adc_base;
//     constexpr auto mV_per_A = 66.0; // from datasheet
//     // constexpr auto mV_per_A = 66.0; //  experimental
//     constexpr auto mA_per_step = mV_per_step / mV_per_A;
//
//     const auto v = mA_per_step;
//     DBG("mA_per_step :%f", v);
//     HAL::println(v);
//     // DBG("mA_per_step :%u / 1000", 1000 * (uint32_t)mA_per_step);
//
//     const auto K = 100;
//     using spn::filter::EWMA;
//     auto f = new EWMA<double>(EWMA<double>::Config{K, false});
//
//     auto spit_a_reading = [heater_current_pin, baseline, f]() {
//         HAL::print("Raw: ");
//         const auto raw_reading = (double)analogRead(heater_current_pin);
//         HAL::println(raw_reading);
//
//         HAL::print("Corrected: ");
//         const auto corrected_reading = raw_reading - baseline;
//         HAL::println(corrected_reading);
//
//         f->new_reading(std::clamp(corrected_reading / adc_base, 0.0, 1.0));
//         HAL::print("Filtered: ");
//         HAL::println(f->value() * adc_base);
//
//         HAL::print("Calculated A: ");
//         HAL::println(corrected_reading * mA_per_step);
//
//         HAL::print("Calculated Filtered A: ");
//         HAL::println(f->value() * adc_base * mA_per_step);
//     };
//
//     for (int i = 0; i < 255; i++) {
//         heater.set_value(float(i) / 255.0);
//         for (int i = 0; i < 100; ++i) {
//             const auto raw_reading = (double)analogRead(heater_current_pin);
//             const auto corrected_reading = raw_reading - baseline;
//             f->new_reading(std::clamp(corrected_reading / adc_base, 0.0, 1.0));
//             delay(10);
//         }
//         spit_a_reading();
//     }
//     HAL::delay_ms(6000);
//
//     for (int i = 255; i > 0; i--) {
//         heater.set_value(float(i) / 255.0);
//         HAL::delay_ms(100);
//         spit_a_reading();
//     }
//
//     HAL::delay_ms(3000);
//     power.set_state(DigitalState::OFF);
// }
// void loop() {
//     DBG("loop");
//     HAL::delay_ms(1000);
// }
//
// #if defined(UNITTEST) || defined(NATIVE)
// int main(int argc, char** argv) {
//     setup();
//     while (true) {
//         loop();
//     }
//     return 0;
// }
// #endif
