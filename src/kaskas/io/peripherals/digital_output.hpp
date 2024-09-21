#pragma once
#include "kaskas/io/peripheral.hpp"
// #include "kaskas/io/provider.hpp"
// #include "kaskas/io/providers/analogue_value.hpp"

#include <spine/core/debugging.hpp>
#include <spine/core/exception.hpp>
#include <spine/filter/implementations/bandpass.hpp>
#include <spine/platform/hal.hpp>
#include <spine/structure/units/si.hpp>

namespace kaskas::io {

class DigitalOutputPeripheral : public HAL::DigitalOutput, public Peripheral {
public:
    explicit DigitalOutputPeripheral(const Config&& cfg) : HAL::DigitalOutput(std::move(cfg)) {}
    ~DigitalOutputPeripheral() override = default;

    void initialize() override { HAL::DigitalOutput::initialize(); }
    void safe_shutdown(bool critical) override { this->set_state(LogicalState::OFF); }
};

} // namespace kaskas::io