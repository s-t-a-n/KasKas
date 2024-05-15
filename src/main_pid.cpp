#include "kaskas/KasKas.hpp"
#include "kaskas/io/heater.hpp"
#include "kaskas/io/implementations/DS18B20_Temp_Probe.hpp"
#include "kaskas/io/implementations/SHT31_TempHumidityProbe.hpp"

#include <spine/controller/pid.hpp>
#include <spine/core/debugging.hpp>
#include <spine/filter/implementations/bandpass.hpp>
#include <spine/filter/implementations/ewma.hpp>
#include <spine/io/sensor.hpp>
#include <spine/platform/hal.hpp>

using spn::controller::Pid;
using spn::filter::EWMA;
Pid* pid = nullptr;
using kaskas::io::DS18B20TempProbe;
using kaskas::io::SHT31TempHumidityProbe;
// DS18B20TempProbe* probe = nullptr;
AnalogueOutput* heater = nullptr;

void setup() {
    HAL::initialize(HAL::Config{.baudrate = 115200});
    HAL::delay(time_ms(100));
    HAL::println("Wake up");

    // auto sht31raw = SHT31(0x44);
    // Wire.begin();
    // Wire.setClock(100000);
    // sht31raw.begin();
    // const auto rawstatus = sht31raw.readStatus();
    // HAL::println(rawstatus);

    auto sht31 = SHT31TempHumidityProbe(SHT31TempHumidityProbe::Config{});
    sht31.initialize();
    DBGF("SHT31 initialized");

    // todo: energybudget; keep track of nominal consumption and reduce power when necessary

    // 231.509, 0.318, 49689.777
    // pid = new Pid(Pid::Config{.free_tunings = Pid::Tunings{.Kp = 47.841, .Ki = 0.376, .Kd = 40.675},
    //                           .in_proximity_tunings =
    //                               Pid::Tunings{.Kp = 60.841, .Ki = 0.376, .Kd = 0.1}, // steadystate: +- 0.3 @ 40 C
    //                           .in_proximity_radius = 5.0,
    //                           .output_lower_limit = 0,
    //                           .output_upper_limit = 255,
    //                           .sample_interval = time_ms(1000)});
    // pid->initialize();
    //
    // using TemperatureSensor = Sensor<DS18B20TempProbe, BandPass>;
    //
    // const auto probe_cfg = DS18B20TempProbe::Config{.pin = D4};
    // // probe = new DS18B20TempProbe(probe_cfg);
    // // probe->initialize();
    //
    // const auto bpf_cfg =
    //     BandPass::Config{.mode = BandPass::Mode::RELATIVE, .mantissa = 1, .decades = 0.01, .offset = 0};
    // // BandPass bpf(bpf_cfg);
    // auto sensor = TemperatureSensor(TemperatureSensor::Config{.sensor_cfg = probe_cfg, .filter_cfg = bpf_cfg});
    // sensor.initialize();
    //
    // const auto probe_first_value = (sensor.src().read() + sensor.src().read()) / 2.0;
    // DBGF("Initiliazing bandpass filter with %f", probe_first_value)
    // sensor.flt().reset_to(probe_first_value);
    //
    // heater = new AnalogueOutput(AnalogueOutput::Config{.pin = D5, .active_on_low = true});
    // heater->initialize();

    const auto sample_interval = time_ms(1000);
    const auto setpoint = 40.0;
    // pid->set_target_setpoint(setpoint);
    using BandPass = spn::filter::BandPass<double>;
    using Heater = kaskas::io::Heater<DS18B20TempProbe>;
    Heater new_heater(Heater::Config{
        .pid_cfg = Pid::Config{.free_tunings = Pid::Tunings{.Kp = 47.841, .Ki = 0.376, .Kd = 40.675},
                               .in_proximity_tunings =
                                   Pid::Tunings{.Kp = 60.841, .Ki = 0.376, .Kd = 0.1}, // steadystate: +- 0.3 @ 40 C
                               .in_proximity_radius = 5.0,
                               .output_lower_limit = 0,
                               .output_upper_limit = 255,
                               .sample_interval = sample_interval},
        .heater_cfg = AnalogueOutput::Config{.pin = D5, .active_on_low = true},
        .tempprobe_cfg = DS18B20TempProbe::Config{.pin = D4},
        .tempprobe_filter_cfg =
            BandPass::Config{.mode = BandPass::Mode::RELATIVE, .mantissa = 1, .decades = 0.01, .offset = 0}});
    new_heater.initialize();
    // const auto probe_read = [&]() {
    // return probe->read()
    // sensor.update();
    // return sensor.value();
    // };

    DBGF("Current temperature is %f C", new_heater.temperature());
    while (true) {
    }
    const auto do_autotune = false;
    const auto autotune_startpoint = setpoint - 5.0;

    // aim to get to target and make sure thermal capacity is emptied before continueing
    // const auto block_until_setpoint = [&](const double target) {
    //     while (new_heater.temperature() < target) {
    //         heater->fade_to(1.0);
    //         DBGF("Waiting until temperature of %f C reaches %f C, saturating thermal capacitance",
    //              new_heater.temperature(), target);
    //         HAL::delay(time_ms(1000));
    //     }
    //     heater->fade_to(0.0);
    //     while (new_heater.temperature() > target) {
    //         DBGF("Waiting until temperature of %f C reaches %f C, unloading thermal capacitance",
    //              new_heater.temperature(), target);
    //         HAL::delay(time_ms(1000));
    //     }
    // };

    if (do_autotune) {
        new_heater.block_until_setpoint(autotune_startpoint);
        // block_until_setpoint(autotune_startpoint);
        DBGF("Fading from %u to: %f", (unsigned)(255.0 * heater->value()), 255.0);
        heater->fade_to(1.0, 0.05, time_ms(500));
        const auto process_setter = [&](double value) { heater->set_value(value); };
        const auto process_getter = [&]() { return new_heater.temperature(); };
        pid->autotune(Pid::TuneConfig{.cycles = 8}, process_setter, process_getter);
    }

    new_heater.block_until_setpoint(35.0);

    // pid->new_reading(new_heater.temperature());
    // heater->fade_to(pid->response() / 255.0, 0.05, time_ms(500));
    // heater->set_value(0.8);
    // heater->fade_to(1.0);
    // assert(heater->value() == 1.0);

    // smooth out responses from pid
    // auto smoother = EWMA<double>(EWMA<double>::Config{.K = 5, .invert = false});
    // Serial.println(pid->response());
    // smoother.new_reading(pid->response() / 255.0);

    while (true) {
        new_heater.update();
        // const auto reading = new_heater.temperature();
        // pid->new_reading(reading);
        //
        // const auto response = pid->response() / 255.0;
        // // smoother.new_reading(response);
        //
        // // const auto smoothed_response = smoother.value();
        // HAL::println(reading);
        // // DBGF("Reading: %f, response: %f, smoothed_response: %f, setpoint: %f", reading, response,
        // // smoothed_response,
        // // setpoint);
        //
        // // heater->set_value(smoothed_response);
        // heater->set_value(response);
        HAL::delay(sample_interval);
    }
}

void loop() {
    DBG("loop");
    HAL::delay(time_ms(1000));
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

//
//    FILE: SHT31_async.ino
//  AUTHOR: Rob Tillaart
// PURPOSE: demo async interface
//     URL: https://github.com/RobTillaart/SHT31

// #include "SHT31.h"
// #include "Wire.h"
//
// #define SHT31_ADDRESS 0x44
//
// uint32_t start;
// uint32_t stop;
// uint32_t cnt;
//
// SHT31 sht(SHT31_ADDRESS); //  uses explicit address
//
// void setup() {
//     Serial.begin(115200);
//     Serial.println(__FILE__);
//     Serial.print("SHT31_LIB_VERSION: \t");
//     Serial.println(SHT31_LIB_VERSION);
//
//     Wire.begin();
//     Wire.setClock(100000);
//     sht.begin();
//
//     uint16_t stat = sht.readStatus();
//     Serial.print(stat, HEX);
//     Serial.println();
//
//     sht.requestData();
//     cnt = 0;
// }
//
// void loop() {
//     uint16_t rawTemperature;
//     uint16_t rawHumidity;
//
//     if (sht.dataReady()) {
//         start = micros();
//         bool success = sht.readData(); //  default = true = fast
//         stop = micros();
//         sht.requestData(); //  request for next sample
//
//         Serial.print("\t");
//         Serial.print(stop - start);
//         Serial.print("\t");
//         if (success == false) {
//             Serial.println("Failed read");
//         } else {
//             rawTemperature = sht.getRawTemperature();
//             rawHumidity = sht.getRawHumidity();
//             Serial.print(rawTemperature, HEX);
//             Serial.print(" = ");
//
//             //  This formula comes from page 14 of the SHT31 datasheet
//             Serial.print(rawTemperature * (175.0 / 65535) - 45, 1);
//             Serial.print("°C\t");
//             Serial.print(sht.getRawHumidity(), HEX);
//             Serial.print(" = ");
//
//             //  This formula comes from page 14 of the SHT31 datasheet
//             Serial.print(rawHumidity * (100.0 / 65535), 1);
//             Serial.print("%\t");
//             Serial.println(cnt);
//             cnt = 0;
//         }
//     }
//     cnt++; //  simulate other activity
// }
//
// //  -- END OF FILE --