// #pragma once
//
// #include "kaskas/io/peripherals/analogue_output.hpp"
//
// #include <DS18B20.h>
// #include <spine/core/debugging.hpp>
// #include <spine/core/exception.hpp>
// #include <spine/core/time.hpp>
//
// namespace kaskas::io {
//
// class Fan : public AnalogueOutputPeripheral {
// public:
//     using AnalogueOutputPeripheral::AnalogueOutputPeripheral;
//
//     AnalogueValue value_provider() const {
//         return {[this]() { return this->value(); }};
//     }
// };
//
// } // namespace kaskas::io