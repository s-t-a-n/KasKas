// #include "kaskas/KasKas.hpp"
// #include "kaskas/io/heater.hpp"
// #include "kaskas/io/implementations/DS18B20_Temp_Probe.hpp"
// #include "kaskas/io/implementations/SHT31_TempHumidityProbe.hpp"
//
// #include <spine/controller/pid.hpp>
// #include <spine/controller/sr_latch.hpp>
// #include <spine/core/debugging.hpp>
// #include <spine/filter/implementations/bandpass.hpp>
// #include <spine/filter/implementations/ewma.hpp>
// #include <spine/io/sensor.hpp>
// #include <spine/platform/hal.hpp>
//
// using spn::controller::PID;
// using spn::filter::EWMA;
// // PID* pid = nullptr;
// using kaskas::io::DS18B20TempProbe;
// using kaskas::io::SHT31TempHumidityProbe;
// // DS18B20TempProbe* probe = nullptr;
// // AnalogueOutput* heater = nullptr;
//
// void setup() {
//     HAL::initialize(HAL::Config{.baudrate = 115200});
//     HAL::delay(time_ms(100));
//     HAL::println("Wake up");
//
//     auto sht31 = SHT31TempHumidityProbe(SHT31TempHumidityProbe::Config{});
//     sht31.initialize();
//     DBGF("SHT31 initialized");
//
//     // todo: energybudget; keep track of nominal consumption and reduce power when necessary
//
//     const auto sample_interval = time_ms(1000);
//     const auto surface_setpoint = 60.0;
//
//     using BandPass = spn::filter::BandPass<double>;
//     using Heater = kaskas::io::Heater<DS18B20TempProbe>;
//     Heater heater(Heater::Config{
//         .pid_cfg =
//             PID::Config{.tunings = PID::Tunings{.Kp = 60.841, .Ki = 0.376, .Kd = 0.1}, // steadystate: +- 0.3 @ 40 C
//                         .output_lower_limit = 0,
//                         .output_upper_limit = 255,
//                         .sample_interval = sample_interval},
//         .heater_cfg = AnalogueOutput::Config{.pin = D5, .active_on_low = true},
//         .tempprobe_cfg = DS18B20TempProbe::Config{.pin = D4},
//         .tempprobe_filter_cfg =
//             BandPass::Config{.mode = BandPass::Mode::RELATIVE, .mantissa = 1, .decades = 0.01, .offset = 0}});
//     heater.initialize();
//     // heater.set_setpoint(surface_setpoint);
//
//     DBGF("Current temperature is %f C", heater.temperature());
//
//     // while (true) {
//     // }
//
//     const auto second_order_sample_interval = time_s(60);
//     auto second_order_pid =
//         PID(PID::Config{.tunings = PID::Tunings{.Kp = 60.841, .Ki = 3.0, .Kd = 0.001}, // steadystate: +- 0.3 @ 40 C
//                         .output_lower_limit = 0,
//                         .output_upper_limit = surface_setpoint,
//                         .sample_interval = second_order_sample_interval});
//     second_order_pid.initialize();
//
//     const auto do_autotune = false;
//     const auto surface_autotune_startpoint = surface_setpoint - 5.0;
//
//     const auto climate_setpoint = 27.0;
//     const auto climate_autotune_startpoint = climate_setpoint - 1.0;
//     second_order_pid.set_target_setpoint(climate_setpoint);
//
//     if (do_autotune) {
//         // while (sht31.read_temperature() > climate_autotune_startpoint) {
//         //     DBGF("Waiting until climate temperature of %f C reaches %f C, unloading thermal capacitance",
//         //          sht31.read_temperature(), climate_autotune_startpoint);
//         //     HAL::delay(time_ms(1000));
//         //
//         const auto update_function = [&]() { heater.update(); };
//         const auto process_setter = [&](double value) {
//             const auto output_value = value * surface_setpoint;
//             heater.set_setpoint(output_value);
//             DBGF("Setting heater to setpoint %f", output_value);
//             HAL::print(heater.temperature());
//             HAL::print("|");
//             HAL::println(sht31.read_temperature());
//         };
//         const auto process_getter = [&]() { return sht31.read_temperature(); };
//         second_order_pid.set_target_setpoint(climate_setpoint);
//         second_order_pid.autotune(PID::TuneConfig{.cycles = 3}, process_setter, process_getter, update_function);
//     }
//
//     // new_heater.block_until_setpoint(35.0);
//
//     heater.set_setpoint(surface_setpoint);
//
//     auto fan = Relay(Relay::Config{.pin_cfg = DigitalOutput::Config{.pin = D7, .active_on_low = false},
//                                    .backoff_time = time_ms(100)});
//     fan.initialize();
//     // fan.set_state(LogicalState::ON);
//
//     // const auto humidity_window_size = time_s(60);
//     //
//     // const auto humidity_sample_interval = time_s(10);
//     // auto humidity_pid =
//     //     PID(PID::Config{.tunings = PID::Tunings{.Kp = 1000, .Ki = 100, .Kd = 0}, // steadystate: +- 0.3 @ 40 C
//     //                     .output_lower_limit = 0,
//     //                     .output_upper_limit = time_ms(humidity_window_size).raw<double>(),
//     //                     .direction = PID::Direction::FORWARD,
//     //                     .sample_interval = humidity_sample_interval});
//     // humidity_pid.initialize();
//     //
//     // const auto humidity_setpoint = 60.0;
//     // humidity_pid.set_target_setpoint(humidity_setpoint);
//     // auto windowStartTime = HAL::millis();
//     using spn::controller::SRLatch;
//
//     // time_ms minimal_on_time = time_ms(0);
//     // time_ms maximal_on_time = time_ms(0);
//     // time_ms minimal_off_time = time_ms(0);
//     // time_ms maximal_off_time = time_ms(0);
//
//     auto humidity_srlatch = SRLatch(SRLatch::Config{.high = 65.40,
//                                                     .low = 40.0,
//                                                     .minimal_on_time = time_s(10),
//                                                     .maximal_on_time = time_s(10),
//                                                     .minimal_off_time = time_s(30),
//                                                     .maximal_off_time = time_s(0)});
//
//     while (true) {
//         const auto humidity = sht31.read_humidity();
//         humidity_srlatch.new_reading(humidity);
//         DBGF("Humidity: %f %%, RV: %i ", humidity, humidity_srlatch.response());
//         fan.set_state(humidity_srlatch.response());
//         //
//         // if (HAL::millis() - windowStartTime > humidity_window_size) {
//         //     // time to shift the Relay Window
//         //     DBGF("expired");
//         //     windowStartTime += humidity_window_size;
//         // }
//         // if (time_ms(humidity_pid.response()) < HAL::millis() - windowStartTime) {
//         //     fan.set_state(LogicalState::ON);
//         //     DBGF("Turning on");
//         // } else
//         //     fan.set_state(LogicalState::OFF);
//
//         heater.set_setpoint(0.0);
//         HAL::delay(sample_interval);
//
//         continue;
//
//         heater.update();
//
//         const auto sht31_temp = sht31.read_temperature();
//         second_order_pid.new_reading(sht31_temp);
//         const auto new_setpoint = second_order_pid.response();
//         heater.set_setpoint(new_setpoint);
//         DBGF("Second order setpoint: %f, heater setpoint: %f", second_order_pid.setpoint(), new_setpoint);
//
//         HAL::print(heater.temperature());
//         HAL::print("|");
//         HAL::println(sht31_temp);
//         HAL::delay(sample_interval);
//     }
// }
//
// void loop() {
//     DBG("loop");
//     HAL::delay(time_ms(1000));
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
//
// //
// //    FILE: SHT31_async.ino
// //  AUTHOR: Rob Tillaart
// // PURPOSE: demo async interface
// //     URL: https://github.com/RobTillaart/SHT31
//
// // #include "SHT31.h"
// // #include "Wire.h"
// //
// // #define SHT31_ADDRESS 0x44
// //
// // uint32_t start;
// // uint32_t stop;
// // uint32_t cnt;
// //
// // SHT31 sht(SHT31_ADDRESS); //  uses explicit address
// //
// // void setup() {
// //     Serial.begin(115200);
// //     Serial.println(__FILE__);
// //     Serial.print("SHT31_LIB_VERSION: \t");
// //     Serial.println(SHT31_LIB_VERSION);
// //
// //     Wire.begin();
// //     Wire.setClock(100000);
// //     sht.begin();
// //
// //     uint16_t stat = sht.readStatus();
// //     Serial.print(stat, HEX);
// //     Serial.println();
// //
// //     sht.requestData();
// //     cnt = 0;
// // }
// //
// // void loop() {
// //     uint16_t rawTemperature;
// //     uint16_t rawHumidity;
// //
// //     if (sht.dataReady()) {
// //         start = micros();
// //         bool success = sht.readData(); //  default = true = fast
// //         stop = micros();
// //         sht.requestData(); //  request for next sample
// //
// //         Serial.print("\t");
// //         Serial.print(stop - start);
// //         Serial.print("\t");
// //         if (success == false) {
// //             Serial.println("Failed read");
// //         } else {
// //             rawTemperature = sht.getRawTemperature();
// //             rawHumidity = sht.getRawHumidity();
// //             Serial.print(rawTemperature, HEX);
// //             Serial.print(" = ");
// //
// //             //  This formula comes from page 14 of the SHT31 datasheet
// //             Serial.print(rawTemperature * (175.0 / 65535) - 45, 1);
// //             Serial.print("°C\t");
// //             Serial.print(sht.getRawHumidity(), HEX);
// //             Serial.print(" = ");
// //
// //             //  This formula comes from page 14 of the SHT31 datasheet
// //             Serial.print(rawHumidity * (100.0 / 65535), 1);
// //             Serial.print("%\t");
// //             Serial.println(cnt);
// //             cnt = 0;
// //         }
// //     }
// //     cnt++; //  simulate other activity
// // }
// //
// // //  -- END OF FILE --