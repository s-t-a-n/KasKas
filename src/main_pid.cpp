// #include "kaskas/KasKas.hpp"
// #include "kaskas/io/implementations/DS18B20_Temp_Probe.hpp"
//
// #include <spine/controller/pid.hpp>
// #include <spine/core/debugging.hpp>
// #include <spine/filter/implementations/ewma.hpp>
//
// using spn::controller::Pid;
// using spn::filter::EWMA;
// Pid* pid = nullptr;
// using kaskas::io::DS18B20TempProbe;
// DS18B20TempProbe* probe = nullptr;
// AnalogueOutput* heater = nullptr;
//
// void setup() {
//     HAL::initialize(HAL::Config{.baudrate = 115200});
//     HAL::println("Wake up");
//
//     // todo: energybudget; keep track of nominal consumption and reduce power when necessary
//
//     // 231.509, 0.318, 49689.777
//     pid = new Pid(Pid::Config{.free_tunings = Pid::Tunings{.Kp = 47.841, .Ki = 0.376, .Kd = 4031.675},
//                               .in_proximity_tunings = Pid::Tunings{.Kp = 60.841, .Ki = 0.376, .Kd = 0.1},
//                               .in_proximity_radius = 5.0,
//                               .output_lower_limit = 0,
//                               .output_upper_limit = 255,
//                               .sample_interval = time_ms(1000)});
//     pid->initialize();
//     probe = new DS18B20TempProbe(DS18B20TempProbe::Config{.pin = D4});
//     probe->initialize();
//
//     heater = new AnalogueOutput(AnalogueOutput::Config{.pin = D5, .active_on_low = true});
//     heater->initialize();
//
//     const auto setpoint = 40.0;
//     pid->set_target_setpoint(setpoint);
//
//     DBGF("Current temperature is %f C", probe->read());
//
//     const auto do_autotune = false;
//     const auto autotune_startpoint = setpoint - 5.0;
//
//     // aim to get to target and make sure thermal capacity is emptied before continueing
//     const auto block_until_setpoint = [&](const double target) {
//         while (probe->read() < target) {
//             heater->fade_to(1.0);
//             DBGF("Waiting until temperature of %f C reaches %f C, saturating thermal capacitance", probe->read(),
//                  target);
//             HAL::delay(time_ms(1000));
//         }
//         heater->fade_to(0.0);
//         while (probe->read() > target) {
//             DBGF("Waiting until temperature of %f C reaches %f C, unloading thermal capacitance", probe->read(),
//                  target);
//             HAL::delay(time_ms(1000));
//         }
//     };
//
//     if (do_autotune) {
//         block_until_setpoint(autotune_startpoint);
//         DBGF("Fading from %u to: %f", (unsigned)(255.0 * heater->value()), 255.0);
//         heater->fade_to(1.0, 0.05, time_ms(500));
//         const auto process_setter = [&](double value) { heater->set_value(value); };
//         const auto process_getter = [&]() { return probe->read(); };
//         pid->autotune(Pid::TuneConfig{.cycles = 8}, process_setter, process_getter);
//     }
//
//     block_until_setpoint(35.0);
//     pid->new_reading(probe->read());
//     DBGF("Fading from %u to: %f", (unsigned)(255.0 * heater->value()), (unsigned)pid->response() / 255.0);
//     heater->fade_to(pid->response() / 255.0, 0.05, time_ms(500));
//     // heater->set_value(0.8);
//     // heater->fade_to(1.0);
//     // assert(heater->value() == 1.0);
//
//     // smooth out responses from pid
//     auto smoother = EWMA<double>(EWMA<double>::Config{.K = 5, .invert = false});
//     // Serial.println(pid->response());
//     smoother.new_reading(pid->response() / 255.0);
//
//     while (true) {
//         const auto reading = probe->read();
//         pid->new_reading(reading);
//
//         const auto response = pid->response() / 255.0;
//         smoother.new_reading(response);
//
//         const auto smoothed_response = smoother.value();
//         HAL::println(reading);
//         // DBGF("Reading: %f, response: %f, smoothed_response: %f, setpoint: %f", reading, response,
//         smoothed_response,
//         // setpoint);
//
//         // heater->set_value(smoothed_response);
//         heater->set_value(response);
//         HAL::delay(time_ms(1000));
//     }
// }
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