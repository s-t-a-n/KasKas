#pragma once
#include "kaskas/io/peripheral.hpp"
#include "kaskas/io/provider.hpp"
#include "kaskas/io/providers/analogue_value.hpp"

#include <spine/core/debugging.hpp>
#include <spine/core/exception.hpp>
#include <spine/core/time.hpp>

namespace kaskas::io {

class AnalogueOutputPeripheral : public AnalogueOutput, public Peripheral {
public:
    explicit AnalogueOutputPeripheral(const Config&& cfg) : HAL::AnalogueOutput(std::move(cfg)) {}
    ~AnalogueOutputPeripheral() override = default;

    void safe_shutdown(bool critical) override { this->fade_to(0.0); }

    AnalogueValue analogue_value_provider() const {
        return {[this]() { return this->value(); }};
    }
};

} // namespace kaskas::io