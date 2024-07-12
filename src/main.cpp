#if !defined(UNITTEST)

#    include "kaskas/io/hardware_stack.hpp"
#    include "kaskas/io/peripherals/DS18B20_Temp_Probe.hpp"
#    include "kaskas/io/peripherals/analogue_input.hpp"
#    include "kaskas/io/peripherals/analogue_output.hpp"
#    include "kaskas/kaskas.hpp"

#    include <spine/controller/pid.hpp>
#    include <spine/core/debugging.hpp>
#    include <spine/filter/implementations/bandpass.hpp>
#    include <spine/filter/implementations/ewma.hpp>
#    include <spine/filter/implementations/invert.hpp>
#    include <spine/filter/implementations/mapped_range.hpp>

using KasKas = kaskas::KasKas;
using HardwareStack = kaskas::io::HardwareStack;

static std::unique_ptr<KasKas> kk;
static std::shared_ptr<HardwareStack> hws;

using kaskas::Events;
using kaskas::io::DS18B20TempProbe;
using kaskas::io::Pump;
using kaskas::io::SHT31TempHumidityProbe;
using kaskas::io::clock::DS3231Clock;
using spn::controller::PID;
using spn::controller::SRLatch;
using spn::core::time::Schedule;
using spn::eventsystem::EventSystem;

using BandPass = spn::filter::BandPass<double>;
using EWMA = spn::filter::EWMA<double>;
using MappedRange = spn::filter::MappedRange<double>;
using Invert = spn::filter::Invert<double>;
using Heater = kaskas::io::Heater;

// volatile uint8_t interruptor = 0;
// DigitalInput ub();

kaskas::io::HardwareStack* S = nullptr;

namespace Peripherals {
enum PeripheralsEnum {
    DS18B20,
    SHT31,
    DS3231,
    SOIL_MOISTURE_SENSOR,
    CLIMATE_FAN,
    HEATING_SURFACE_FAN,
    HEATING_ELEMENT,
    HEATING_POWER_RELAY,
    VIOLET_SPECTRUM_RELAY,
    BROAD_SPECTRUM_RELAY,
    PUMP_RELAY,
    SIZE,
};
}

#    include "kaskas/prompt/prompt.hpp"

using namespace kaskas;

// class MockController {
// public:
//     std::unique_ptr<prompt::RPCRecipe> rpc_recipe() {
//         using namespace prompt;
//         using kaskas::prompt::OptStringView;
//
//         using spn::structure::Array;
//         auto model = std::make_unique<RPCRecipe>(
//             RPCRecipe("MOC", //
//                       {
//                           RPCModel("roVariable",
//                                    [this](const OptStringView&) {
//                                        DBGF("roVariable accessed");
//                                        return RPCResult(std::to_string(roVariable));
//                                    }),
//                           RPCModel("rwVariable",
//                                    [this](const OptStringView& s) {
//                                        const auto s_str = s ? std::string{*s} : std::string();
//                                        DBGF("rwVariable accessed with arg: {%s}", s_str.c_str());
//                                        rwVariable = s ? std::stod(std::string(*s)) : rwVariable;
//                                        return RPCResult(std::to_string(rwVariable));
//                                    }), //
//                           RPCModel("foo",
//                                    [this](const OptStringView& s) {
//                                        if (!s || s->length() == 0) {
//                                            return RPCResult(RPCResult::State::BAD_INPUT);
//                                        }
//                                        const auto s_str = s ? std::string{*s} : std::string();
//                                        DBGF("foo called with arg: '%s'", s_str.c_str());
//
//                                        return RPCResult(std::to_string(foo(std::stod(s_str))));
//                                    }),
//                       }));
//         return std::move(model);
//     }
//
//     const double roVariable = 42;
//     double rwVariable = 42;
//
//     double foo(double variable) { return roVariable + rwVariable + variable; }
// };

// unsigned long last_free_memory = 0;

void setup() {
    HAL::delay(time_s(2)); // give time for console to attach before first output
    HAL::initialize(HAL::Config{.baudrate = 115200});
    HAL::println("Wake up");

    // static volatile int ctr = 0;
    // const auto cb = []() { ctr++; };
    // Interrupt a(
    //     Interrupt::Config{.pin = 2, .mode = Interrupt::TriggerType::FALLING_EDGE, .pull_up = false, .callback = cb});
    // a.initialize();
    // a.attach_interrupt();
    //
    // io::Relay b(io::Relay::Config{.pin_cfg = HAL::DigitalOutput::Config{.pin = 13, .active_on_low = true},
    //                               .backoff_time = time_ms(1000)});
    // b.initialize();
    //
    // HAL::delay(time_s(10));
    // b.set_state(LogicalState::ON);
    // HAL::delay(time_s(3));
    // b.set_state(LogicalState::OFF);
    //
    // // for (int i = 0; i < 3; ++i) {
    // //     HAL::delay(time_s(1));
    // // }
    // DBGF("ctr: %i", ctr);
    // while (true) {
    // };

    //     {
    //         HAL::delay(time_s(2));
    //
    // #    include <DS3231-RTC.h>
    // #    include <Wire.h>
    //
    //         DS3231::DS3231 myRTC;
    //
    //         byte year;
    //         byte month;
    //         byte date;
    //         byte dOW;
    //         byte hour;
    //         byte minute;
    //         byte second;
    //
    //         const auto getDateStuff = [](byte& year, byte& month, byte& date, byte& dOW, byte& hour, byte& minute,
    //                                      byte& second) {
    //             // Call this if you notice something coming in on
    //             // the serial port. The stuff coming in should be in
    //             // the order YYMMDDwHHMMSS, with an 'x' at the end.
    //             boolean gotString = false;
    //             char inChar;
    //             byte temp1, temp2;
    //             char inString[20];
    //
    //             byte j = 0;
    //             while (!gotString) {
    //                 if (Serial.available()) {
    //                     inChar = Serial.read();
    //                     inString[j] = inChar;
    //                     j += 1;
    //                     if (inChar == 'x') {
    //                         gotString = true;
    //                     }
    //                 }
    //             }
    //             Serial.println(inString);
    //             // Read year first
    //             temp1 = (byte)inString[0] - 48;
    //             temp2 = (byte)inString[1] - 48;
    //             year = temp1 * 10 + temp2;
    //             // now month
    //             temp1 = (byte)inString[2] - 48;
    //             temp2 = (byte)inString[3] - 48;
    //             month = temp1 * 10 + temp2;
    //             // now date
    //             temp1 = (byte)inString[4] - 48;
    //             temp2 = (byte)inString[5] - 48;
    //             date = temp1 * 10 + temp2;
    //             // now Day of Week
    //             dOW = (byte)inString[6] - 48;
    //             // now hour
    //             temp1 = (byte)inString[7] - 48;
    //             temp2 = (byte)inString[8] - 48;
    //             hour = temp1 * 10 + temp2;
    //             // now minute
    //             temp1 = (byte)inString[9] - 48;
    //             temp2 = (byte)inString[10] - 48;
    //             minute = temp1 * 10 + temp2;
    //             // now second
    //             temp1 = (byte)inString[11] - 48;
    //             temp2 = (byte)inString[12] - 48;
    //             second = temp1 * 10 + temp2;
    //         };
    //
    //         Wire.begin();
    //
    //         Serial.println("order: YYMMDDwHHMMSS, with an 'x' at the end");
    //         while (true) {
    //             // If something is coming in on the serial line, it's
    //             // a time correction so set the clock accordingly.
    //             if (Serial.available()) {
    //                 getDateStuff(year, month, date, dOW, hour, minute, second);
    //
    //                 myRTC.setClockMode(false); // set to 24h
    //                 // setClockMode(true); // set to 12h
    //
    //                 myRTC.setYear(year);
    //                 myRTC.setMonth(month);
    //                 myRTC.setDate(date);
    //                 myRTC.setDoW(dOW);
    //                 myRTC.setHour(hour);
    //                 myRTC.setMinute(minute);
    //                 myRTC.setSecond(second);
    //
    //                 // Test of alarm functions
    //                 // set A1 to one minute past the time we just set the clock
    //                 // on current day of week.
    //                 myRTC.setA1Time(dOW, hour, minute + 1, second, 0x0, true, false, false);
    //                 // set A2 to two minutes past, on current day of month.
    //                 myRTC.setA2Time(date, hour, minute + 2, 0x0, false, false, false);
    //                 // Turn on both alarms, with external interrupt
    //                 myRTC.turnOnAlarm(1);
    //                 myRTC.turnOnAlarm(2);
    //             }
    //             delay(1000);
    //         }
    //     }

    // {
    //     auto a = HAL::UART(HAL::UART::Config{&Serial});
    //     a.initialize();
    //     a.write((uint8_t*)"ollah", 5);
    //     DBGF("avail: %i", a.available());
    //     return;
    // }

    // {
    //     HAL::delay(time_s(3));
    //
    //     auto pump = io::Relay(io::Relay::Config{.pin_cfg = DigitalOutput::Config{.pin = 13, .active_on_low = true},
    //                                             .backoff_time = time_s(10)});
    //     pump.initialize();
    //
    //     HAL::delay(time_s(3));
    //     pump.set_state(LogicalState::ON);
    //     HAL::delay(time_s(10));
    //     pump.set_state(LogicalState::OFF);
    //     return;
    // }

    // {
    //     HAL::delay(time_s(3));
    //     auto power = io::Relay(io::Relay::Config{.pin_cfg = DigitalOutput::Config{.pin = 4, .active_on_low = true},
    //                                              .backoff_time = time_s(10)});
    //     power.initialize();
    //     power.set_state(LogicalState::ON);
    //
    //     auto fan = HAL::AnalogueOutput(HAL::AnalogueOutput::Config{.pin = 5, .active_on_low = true});
    //     fan.initialize();
    //
    //     auto fan2 = HAL::AnalogueOutput(HAL::AnalogueOutput::Config{.pin = 7, .active_on_low = true});
    //     fan2.initialize();
    //     fan2.set_value(LogicalState::ON);
    //
    //     // fan.set_value(0.95);
    //     // HAL::delay(time_s(100));
    //
    //     for (int i = 0; i < 255; ++i) {
    //         fan.set_value(i / 255.0);
    //         DBGF("setting fan to : %f", i / 255.0);
    //         HAL::delay_ms(250);
    //     }
    //
    //     for (int i = 0; i < 255; ++i) {
    //         fan.set_value((255 - i) / 255.0);
    //         DBGF("setting fan to : %f", (255 - i) / 255.0);
    //         HAL::delay_ms(50);
    //     }
    //
    //     HAL::delay(time_s(5));
    //     fan.set_value(0);
    //     return;
    // }

    // const auto do_thing = [&]() {
    //     last_free_memory = HAL::free_memory();
    //     {
    //         using namespace kaskas::prompt;
    //
    //         auto prompt_cfg = Prompt::Config{.message_length = 64, .pool_size = 20};
    //
    //         auto dl = std::make_shared<MockDatalink>(
    //             MockDatalink::Config{.message_length = prompt_cfg.message_length, .pool_size =
    //             prompt_cfg.pool_size});
    //         auto prompt = Prompt(std::move(prompt_cfg));
    //         prompt.hotload_datalink(dl);
    //
    //         auto mc = MockController();
    //         prompt.hotload_rpc_recipe(mc.rpc_recipe());
    //
    //         prompt.initialize();
    //
    //         auto bufferpool = dl->bufferpool();
    //         assert(bufferpool);
    //         {
    //             const auto test_input = "MOC!foo:1";
    //             const auto test_input_len = strlen(test_input);
    //             auto buf = bufferpool->acquire();
    //             assert(test_input_len < buf->capacity);
    //             strncpy(buf->raw, test_input, buf->capacity);
    //             buf->length = test_input_len;
    //
    //             auto msg = Message::from_buffer(std::move(buf));
    //             assert(msg != std::nullopt);
    //
    //             const auto s1 = std::string(msg->cmd());
    //             const auto s2 = std::string(msg->operant());
    //             const auto s3 = std::string(*msg->key());
    //             DBGF("Message before injection: %s:%s:%s", s1.c_str(), s2.c_str(), s3.c_str());
    //
    //             // assert(msg->() != nullptr);
    //
    //             dl->inject_message(*msg);
    //             prompt.update();
    //
    //             const auto reply = dl->extract_reply();
    //             if (reply) {
    //                 const auto sk = std::string(*reply->value());
    //                 DBGF("%s", sk.c_str());
    //                 DBGF("received a reply: '%s'", reply->as_string().c_str());
    //                 assert(reply->as_string() == std::string_view("MOC<0:85.000000"));
    //             }
    //         }
    //
    //         DBGF("hello");
    //         // return;
    //     }
    //     DBGF("bye");
    //
    //     DBGF("last: %i, current: %i", last_free_memory, HAL::free_memory());
    //     last_free_memory = HAL::free_memory();
    //     // assert(last_free_memory == HAL::free_memory());
    // };
    // do_thing();
    // do_thing();
    // do_thing();
    // do_thing();
    // return;

    // {
    //     using namespace kaskas::prompt;
    //     const auto message_length = 128;
    //     const auto message_pool_size = 16;
    //
    //     auto dl = std::make_unique<SerialDatalink>(
    //         SerialDatalink::Config{.message_length = message_length, .pool_size = message_pool_size},
    //         HAL::UART(HAL::UART::Config{&Serial}));
    //     auto prompt = std::make_unique<Prompt>(Prompt::Config{}, std::move(dl));
    //
    //     prompt->initialize();
    // }

    // return;

    {
        using namespace kaskas::io;

        auto stack_cfg = HardwareStack::Config{.alias = "IO",
                                               .max_providers = ENUM_IDX(DataProviders::SIZE),
                                               .max_peripherals = Peripherals::SIZE};

        auto sf = HardwareStackFactory(std::move(stack_cfg));

        // Initialize and hotload peripherals and providers

        {
            const auto cfg = SHT31TempHumidityProbe::Config{.sampling_interval = time_s(1)};
            auto peripheral = std::make_unique<SHT31TempHumidityProbe>(std::move(cfg));
            auto temperature_provider = std::make_shared<AnalogueSensor>(peripheral->temperature_provider());
            auto humidity_provider = std::make_shared<AnalogueSensor>(peripheral->humidity_provider());

            sf.hotload_provider(DataProviders::CLIMATE_TEMP, std::move(temperature_provider));
            sf.hotload_provider(DataProviders::CLIMATE_HUMIDITY, std::move(humidity_provider));
            sf.hotload_peripheral(Peripherals::SHT31, std::move(peripheral));
        }

        {
            const auto cfg = DS3231Clock::Config{.update_interval = time_s(1)};
            auto peripheral = std::make_unique<DS3231Clock>(std::move(cfg));
            auto clock_provider = std::make_shared<Clock>(peripheral->clock_provider());
            auto temperature_provider = std::make_shared<AnalogueSensor>(peripheral->temperature_provider());

            sf.hotload_provider(DataProviders::CLOCK, std::move(clock_provider));
            sf.hotload_provider(DataProviders::AMBIENT_TEMP, std::move(temperature_provider));
            sf.hotload_peripheral(Peripherals::DS3231, std::move(peripheral));
        }

        {
            // constexpr double moisture_sensor_limits[] = {0.4, 0.6}; // experimentally obtained, low = wet, high = dry
            constexpr double moisture_sensor_limits[] = {0.3, 0.7}; // experimentally obtained, low = wet, high = dry
            // constexpr double moisture_sensor_limits[] = {0.0, 1.0}; // experimentally obtained, low = wet, high = dry
            const auto cfg =
                AnalogueInputPeripheral::Config{.input_cfg = HAL::AnalogueInput::Config{.pin = A1, .pull_up = false},
                                                .sampling_interval = time_s(10),
                                                .number_of_filters = 4};
            auto peripheral = std::make_unique<AnalogueInputPeripheral>(std::move(cfg));
            peripheral->attach_filter(BandPass::Broad());
            peripheral->attach_filter(EWMA::Long());
            peripheral->attach_filter(Invert::Invertor());
            peripheral->attach_filter(MappedRange::Percentage(moisture_sensor_limits[0], moisture_sensor_limits[1]));
            auto moisture_provider = std::make_shared<AnalogueSensor>(peripheral->analogue_value_provider());

            sf.hotload_provider(DataProviders::SOIL_MOISTURE, std::move(moisture_provider));
            sf.hotload_peripheral(Peripherals::SOIL_MOISTURE_SENSOR, std::move(peripheral));
        }

        {
            const auto sample_speed = time_ms(50);
            const auto cfg = AnalogueOutputPeripheral::Config{.pin = 5, .active_on_low = true};
            auto peripheral = std::make_unique<AnalogueOutputPeripheral>(std::move(cfg), sample_speed);
            auto state_provider = std::make_shared<AnalogueActuator>(peripheral->analogue_output_provider());

            sf.hotload_provider(DataProviders::CLIMATE_FAN, std::move(state_provider));
            sf.hotload_peripheral(Peripherals::CLIMATE_FAN, std::move(peripheral));
        }

        {
            const auto cfg = DS18B20TempProbe::Config{.pin = 3, .sampling_interval = time_ms(250)};
            auto peripheral = std::make_unique<DS18B20TempProbe>(std::move(cfg));
            auto provider = std::make_shared<AnalogueSensor>(peripheral->temperature_provider());

            sf.hotload_provider(DataProviders::HEATING_SURFACE_TEMP, std::move(provider));
            sf.hotload_peripheral(Peripherals::DS18B20, std::move(peripheral));
        }

        {
            const auto cfg = AnalogueOutputPeripheral::Config{.pin = 7, .active_on_low = true};
            auto peripheral = std::make_unique<AnalogueOutputPeripheral>(std::move(cfg));
            auto state_provider = std::make_shared<AnalogueActuator>(peripheral->analogue_output_provider());

            sf.hotload_provider(DataProviders::HEATING_SURFACE_FAN, std::move(state_provider));
            sf.hotload_peripheral(Peripherals::HEATING_SURFACE_FAN, std::move(peripheral));
        }

        {
            const auto cfg = AnalogueOutput::Config{.pin = 6, .active_on_low = true};
            auto peripheral = std::make_unique<AnalogueOutputPeripheral>(std::move(cfg));
            auto state_provider = std::make_shared<AnalogueActuator>(peripheral->analogue_output_provider());

            sf.hotload_provider(DataProviders::HEATING_ELEMENT, std::move(state_provider));
            sf.hotload_peripheral(Peripherals::HEATING_ELEMENT, std::move(peripheral));
        }

        {
            const auto cfg = Relay::Config{.pin_cfg = DigitalOutput::Config{.pin = 4, .active_on_low = true},
                                           .backoff_time = time_s(10)};
            auto peripheral = std::make_unique<Relay>(std::move(cfg));
            auto state_provider = std::make_shared<DigitalActuator>(peripheral->state_provider());

            sf.hotload_provider(DataProviders::HEATING_POWER, std::move(state_provider));
            sf.hotload_peripheral(Peripherals::HEATING_POWER_RELAY, std::move(peripheral));
        }

        {
            const auto cfg = Relay::Config{.pin_cfg = DigitalOutput::Config{.pin = A5, .active_on_low = true},
                                           .backoff_time = time_ms(1000)};
            auto peripheral = std::make_unique<Relay>(std::move(cfg));
            auto state_provider = std::make_shared<DigitalActuator>(peripheral->state_provider());

            sf.hotload_provider(DataProviders::REDBLUE_SPECTRUM, std::move(state_provider));
            sf.hotload_peripheral(Peripherals::VIOLET_SPECTRUM_RELAY, std::move(peripheral));
        }

        {
            const auto cfg = Relay::Config{.pin_cfg = DigitalOutput::Config{.pin = 12, .active_on_low = true},
                                           .backoff_time = time_ms(1000)};
            auto peripheral = std::make_unique<Relay>(std::move(cfg));
            auto state_provider = std::make_shared<DigitalActuator>(peripheral->state_provider());

            sf.hotload_provider(DataProviders::FULL_SPECTRUM, std::move(state_provider));
            sf.hotload_peripheral(Peripherals::BROAD_SPECTRUM_RELAY, std::move(peripheral));
        }

        {
            const auto cfg = Relay::Config{.pin_cfg = DigitalOutput::Config{.pin = 13, .active_on_low = true},
                                           .backoff_time = time_ms(1000)};
            auto peripheral = std::make_unique<Relay>(std::move(cfg));
            auto state_provider = std::make_shared<DigitalActuator>(peripheral->state_provider());

            sf.hotload_provider(DataProviders::PUMP, std::move(state_provider));
            sf.hotload_peripheral(Peripherals::PUMP_RELAY, std::move(peripheral));
        }

        hws = sf.hardware_stack();
        hws->initialize();
    }

    {
        auto esc_cfg = EventSystem::Config{.events_count = static_cast<size_t>(Events::Size),
                                           .events_cap = 128,
                                           .handler_cap = 2,
                                           .delay_between_ticks = true,
                                           .min_delay_between_ticks = time_ms{1},
                                           .max_delay_between_ticks = time_ms{1000}};
        constexpr bool enable_prompt = true;
        auto prompt_cfg = enable_prompt
                              ? std::make_optional(kaskas::Prompt::Config{.message_length = 64, .pool_size = 10})
                              : std::nullopt;
        auto kk_cfg = KasKas::Config{.es_cfg = esc_cfg, .component_cap = 16, .prompt_cfg = prompt_cfg};
        kk = std::make_unique<KasKas>(hws, kk_cfg);
    }

    // todo: fully offload this to yaml file which is loaded in through pyserial API
    {
        using kaskas::component::ClimateControl;

        const auto ventilation_sample_interval = time_s(1);
        const auto heating_sample_interval = time_ms(1000);
        const auto max_heater_setpoint = 40.0; // maximum allowed heater setpoint

        auto cc_cfg = ClimateControl::Config{
            .hws_power_idx = ENUM_IDX(DataProviders::HEATING_POWER),
            .clock_idx = ENUM_IDX(DataProviders::CLOCK),

            .ventilation =
                ClimateControl::Config::Ventilation{
                    .hws_climate_fan_idx = ENUM_IDX(DataProviders::CLIMATE_FAN),
                    .climate_humidity_idx = ENUM_IDX(DataProviders::CLIMATE_HUMIDITY),
                    // .climate_fan_pid = PID::Config{.tunings = PID::Tunings{.Kp = 34.57, .Ki = 2.71, .Kd = 0},
                    .climate_fan_pid = PID::Config{.tunings = PID::Tunings{.Kp = 37.13, .Ki = 2.17, .Kd = 0},
                                                   .output_lower_limit = 20,
                                                   .output_upper_limit = 90,
                                                   .sample_interval = ventilation_sample_interval,
                                                   .direction = PID::Direction::FORWARD},
                    .minimal_duty_cycle = 0.21,
                    .schedule_cfg =
                        Schedule::Config{
                            .blocks = {Schedule::Block{.start = time_h(0), .duration = time_h(24), .value = 70.0}}},
                    .check_interval = ventilation_sample_interval},

            .heating = ClimateControl::Config::Heating{
                .heating_element_fan_idx = ENUM_IDX(DataProviders::HEATING_SURFACE_FAN),
                .heating_element_temp_sensor_idx = ENUM_IDX(DataProviders::HEATING_SURFACE_TEMP),
                .climate_temp_sensor_idx = ENUM_IDX(DataProviders::CLIMATE_TEMP),
                .ambient_temp_idx = ENUM_IDX(DataProviders::AMBIENT_TEMP),
                .heater_cfg =
                    Heater::Config{.pid_cfg =
                                       PID::Config{//
                                                   .tunings = PID::Tunings{.Kp = 62.590051, .Ki = 0.152824, .Kd = 0},
                                                   .output_lower_limit = 0,
                                                   .output_upper_limit = 255,
                                                   .sample_interval = heating_sample_interval},
                                   .max_heater_setpoint = max_heater_setpoint,
                                   .heating_surface_temperature_idx = ENUM_IDX(DataProviders::HEATING_SURFACE_TEMP),
                                   .heating_element_idx = ENUM_IDX(DataProviders::HEATING_ELEMENT),
                                   .climate_trp_cfg = Heater::ThermalRunAway::Config{.stable_timewindow = time_m(20),
                                                                                     .heating_minimal_rising_c = 0.1,
                                                                                     .heating_minimal_dropping_c = 0.01,
                                                                                     .heating_timewindow = time_m(45)}},
                .schedule_cfg =
                    Schedule::Config{
                        .blocks = {Schedule::Block{.start = time_h(0), .duration = time_h(7), .value = 16.0},
                                   Schedule::Block{.start = time_h(7), .duration = time_h(2), .value = 18.0},
                                   Schedule::Block{.start = time_h(9), .duration = time_h(1), .value = 20.0},
                                   Schedule::Block{.start = time_h(10), .duration = time_h(1), .value = 22.0},
                                   Schedule::Block{.start = time_h(11), .duration = time_h(1), .value = 24.0},
                                   Schedule::Block{.start = time_h(12),
                                                   .duration = time_h(8),
                                                   .value = 24.0}, // 24.0 for seedling, 27.0 for plant
                                   Schedule::Block{.start = time_h(20), .duration = time_h(2), .value = 24.0},
                                   Schedule::Block{.start = time_h(22), .duration = time_h(2), .value = 16.0}}},
                .check_interval = heating_sample_interval}};

        auto ventilation = std::make_unique<ClimateControl>(*hws, cc_cfg);
        kk->hotload_component(std::move(ventilation));
    }

    {
        using kaskas::component::Growlights;
        auto growlights_cfg = Growlights::Config{
            .redblue_spectrum_actuator_idx = ENUM_IDX(DataProviders::REDBLUE_SPECTRUM),
            .redblue_spectrum_schedule =
                Schedule::Config{
                    .blocks =
                        {
                            Schedule::Block{.start = time_h(8), .duration = time_h(12), .value = LogicalState::ON},
                            Schedule::Block{.start = time_h(20), .duration = time_h(12), .value = LogicalState::OFF},
                        }},

            .full_spectrum_actuator_idx = ENUM_IDX(DataProviders::FULL_SPECTRUM),
            .full_spectrum_schedule =
                Schedule::Config{
                    .blocks =
                        {
                            Schedule::Block{.start = time_h(6), .duration = time_h(16), .value = LogicalState::ON},
                            Schedule::Block{.start = time_h(22), .duration = time_h(8), .value = LogicalState::OFF},
                        }},
            .clock_idx = ENUM_IDX(DataProviders::CLOCK)};

        auto growlights = std::make_unique<Growlights>(*hws, growlights_cfg);
        kk->hotload_component(std::move(growlights));
    }

    {
        auto pump_cfg = Pump::Config{
            //
            .pump_actuator_idx = ENUM_IDX(DataProviders::PUMP),
            .interrupt_cfg =
                Interrupt::Config{
                    .pin = 2,
                    .mode = Interrupt::TriggerType::FALLING_EDGE,
                    .pull_up = false,
                },
            .ml_pulse_calibration = 25.8, // 25.8
            .reading_interval = time_ms(250), //
            .pump_timeout = time_s(10),
        };

        using kaskas::component::Fluidsystem;
        auto fluidsystem_cfg = Fluidsystem::Config{.pump_cfg = pump_cfg, //
                                                   .ground_moisture_sensor_idx = ENUM_IDX(DataProviders::SOIL_MOISTURE),
                                                   .clock_idx = ENUM_IDX(DataProviders::CLOCK),
                                                   .ground_moisture_target = 40, // target moisture percentage
                                                   .max_dosis_ml = 100,
                                                   .time_of_injection = time_h(8),
                                                   .delay_before_effect_evaluation = time_h(2)};
        auto fluidsystem = std::make_unique<Fluidsystem>(*hws, fluidsystem_cfg);
        kk->hotload_component(std::move(fluidsystem));
    }

    {
        using kaskas::component::UI;
        auto ui_cfg =
            UI::Config{//
                       .signaltower_cfg =
                           Signaltower::Config{
                               .pin_red = DigitalOutput(DigitalOutput::Config{.pin = 10, .active_on_low = false}),
                               .pin_yellow = DigitalOutput(DigitalOutput::Config{.pin = 9, .active_on_low = false}),
                               .pin_green = DigitalOutput(DigitalOutput::Config{.pin = 8, .active_on_low = false}),
                           },
                       .userbutton_cfg = DigitalInput::Config{.pin = PC13, .pull_up = false}};
        auto ui = std::make_unique<UI>(*hws, ui_cfg);
        ui->hotload_prompt(kk->prompt());
        kk->hotload_component(std::move(ui));
    }

    {
        using kaskas::component::Hardware;
        auto hardware_cfg = Hardware::Config{};

        auto hardware = std::make_unique<Hardware>(*hws, hardware_cfg);
        kk->hotload_component(std::move(hardware));
    }

    {
        static constexpr auto datasources = {DataProviders::CLIMATE_TEMP,
                                             DataProviders::HEATING_SURFACE_TEMP,
                                             DataProviders::AMBIENT_TEMP,
                                             DataProviders::HEATING_SETPOINT,
                                             DataProviders::HEATING_ELEMENT,
                                             DataProviders::CLIMATE_HUMIDITY,
                                             DataProviders::CLIMATE_HUMIDITY_SETPOINT,
                                             DataProviders::CLIMATE_FAN,
                                             DataProviders::SOIL_MOISTURE,
                                             DataProviders::SOIL_MOISTURE_SETPOINT};

        using kaskas::component::DataAcquisition;
        auto cfg = DataAcquisition::Config{.initial_warm_up_time = time_s(30), .active_dataproviders = datasources};

        auto ctrl = std::make_unique<DataAcquisition>(*hws, cfg);
        kk->hotload_component(std::move(ctrl));
    }

    // DBGF("Memory available: %i", HAL::free_memory());

    // {
    //     auto recipes = hws->cookbook().extract_recipes();
    //     for (auto& r : recipes) {
    //         // const auto cmdstr = std::string(r->command());
    //         // DBGF("Recipe has command: %s", cmdstr.c_str());
    //         // for (const auto& m : r->models()) {
    //         //     const auto recipe_name = std::string(m.name());
    //         //     DBGF("-> has: %s", recipe_name.c_str());
    //         // }
    //         kk->hotload_rpc_recipe(std::move(r));
    //     }
    // }

    kk->initialize();

    // const auto clock = hws->clock(ENUM_IDX(DataProviders::CLOCK));
    // const auto now = clock.now();
    //
    // const auto time_from_now = time_m(clock.time_until_next_occurence(time_h(20)));
    // DBGF("t: %u", time_from_now.printable());
    // while (true) {
    //     HAL::delay_ms(100);
    // }
}
void loop() {
    if (kk == nullptr) {
        DBG("loop");
        // DBGF("interruptor: %i", interruptor);
        // DBGF("ub: %i", ub.state() == LogicalState::ON ? 1 : 0);
        HAL::delay(time_ms(1000));

        // const auto t = S->temperature(HEATER_SURFACE_TEMP);
        // DBGF("HEATER_SURFACE_TEMP : %f", t.value());
        //
        // const auto tcc = S->temperature(CLIMATE_TEMP);
        // DBGF("CLIMATE_TEMP : %f", tcc.value());
        //
        // const auto hcc = S->humidity(CLIMATE_HUMIDITY);
        // DBGF("CLIMATE_HUMIDITY : %f", hcc.value());
        //
        // const auto clock = S->clock(CLOCK);
        // const auto dt = clock.now();
        // const auto at = S->temperature(AMBIENT_TEMP);
        //
        // DBGF("CLOCK: %.2i:%.2i", dt.getHour(), dt.getMinute());
        // DBGF("CLOCK epoch: %i", static_cast<unsigned long>(clock.epoch()));
        // DBGF("AMBIENT_TEMP : %f", at.value());
        //
        // const auto sm = S->moisture(SOIL_MOISTURE);
        // DBGF("SOIL_MOISTURE : %f", sm.value());
        // S->update_all();

    } else {
        kk->loop();
        // DBG("loop");
    }
}

#    if defined(UNITTEST) || defined(NATIVE)
int main(int argc, char** argv) {
    setup();
    while (true) {
        loop();
    }
    return 0;
}
#    endif

#endif
