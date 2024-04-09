#pragma once

#include "kaskas/io/peripheral.hpp"
#include "kaskas/io/provider.hpp"
#include "kaskas/io/providers/analogue.hpp"
#include "kaskas/io/providers/clock.hpp"
#include "kaskas/io/providers/digital.hpp"
#include "kaskas/io/software_stack.hpp"
#include "kaskas/prompt/rpc/cookbook.hpp"

#include <spine/structure/array.hpp>

#include <memory>

namespace kaskas::io {

using spn::structure::Array;

class HardwareStackFactory;

/// Encapsulate peripherals through provider interfaces
/// Responsible for:
/// - initialize and maintain hardware peripherals
/// - update values at set intervals
/// - supply provider interfaces for consumers of data
class HardwareStack : public VirtualStack {
public:
    using Idx = uint16_t;

    struct Config {
        std::string_view alias;
        Idx max_providers = 16;
        Idx max_peripherals = 8;
    };

    HardwareStack(const Config&& cfg)
        : VirtualStack({.alias = cfg.alias, .max_providers = cfg.max_providers}), _cfg(std::move(cfg)),
          _peripherals(_cfg.max_peripherals) {}

public:
    void initialize() {
        for (auto& p : _peripherals) {
            if (p) {
                p->initialize();
            }
        }
    }

    /// Update all peripherals that need an update, respecting the peripheral's `sampling_time`
    void update_all() {
        for (auto& p : _peripherals) {
            if (p && p->needs_update()) {
                p->update();
            }
        }
    }

    /// Safely shutdown all peripherals
    void safe_shutdown(bool critical = false) {
        for (auto& p : _peripherals) {
            if (p) {
                HAL::delay(time_ms(100));
                p->safe_shutdown(critical);
            }
        }
    }

    /// Returns the time until the needed update of a peripheral
    time_ms time_until_next_update() {
        time_ms interval = time_ms(INT32_MAX);
        for (auto& p : _peripherals) {
            if (p && p->is_updateable() && p->time_until_next_update() < interval) {
                interval = std::max(p->time_until_next_update(), time_ms(1));
                assert(interval > time_ms(0));
            }
        }
        assert(interval != time_ms(INT32_MAX));
        return interval;
    }

public:
    const AnalogueSensor& analog_sensor(Idx sensor_idx) {
        assert(_providers[sensor_idx]);
        return *reinterpret_cast<AnalogueSensor*>(_providers[sensor_idx].get());
    }

    const DigitalSensor& digital_sensor(Idx sensor_idx) {
        assert(_providers[sensor_idx]);
        return *reinterpret_cast<DigitalSensor*>(_providers[sensor_idx].get());
    }

    AnalogueActuator& analogue_actuator(Idx output_idx) {
        assert(_providers[output_idx]);
        return *reinterpret_cast<AnalogueActuator*>(_providers[output_idx].get());
    }

    DigitalActuator& digital_actuator(Idx output_idx) {
        assert(_providers[output_idx]);
        return *reinterpret_cast<DigitalActuator*>(_providers[output_idx].get());
    }

    const Clock& clock(Idx clock_idx) {
        assert(_providers[clock_idx]);
        return *reinterpret_cast<Clock*>(_providers[clock_idx].get());
    }

private:
    const Config _cfg;

    Array<std::unique_ptr<Peripheral>> _peripherals;

    friend HardwareStackFactory;
};

class HardwareStackFactory : public VirtualStackFactory {
public:
    HardwareStackFactory(const HardwareStack::Config&& cfg)
        : VirtualStackFactory(std::make_shared<HardwareStack>(std::move(cfg))) {}

    void hotload_peripheral(uint8_t peripheral_id, std::unique_ptr<Peripheral> peripheral) {
        assert(peripheral_id < hardware_stack()->_cfg.max_peripherals);
        assert(hardware_stack()->_peripherals[peripheral_id] == nullptr);
        hardware_stack()->_peripherals[peripheral_id] = std::move(peripheral);
    }

    std::shared_ptr<HardwareStack> hardware_stack() const { return std::static_pointer_cast<HardwareStack>(_stack); }

private:
};

} // namespace kaskas::io
