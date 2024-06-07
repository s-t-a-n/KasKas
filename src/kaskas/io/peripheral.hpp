#pragma once

#include <spine/core/time.hpp>
#include <spine/core/timers.hpp>
#include <spine/structure/array.hpp>

#include <optional>

namespace kaskas::io {

using spn::core::time::IntervalTimer;

/// Encapsalates a single hardware peripheral
class Peripheral {
public:
    explicit Peripheral(const std::optional<time_ms>& sampling_interval = std::nullopt)
        : _timer(sampling_interval ? std::make_optional(IntervalTimer(sampling_interval.value())) : std::nullopt) {}

    virtual ~Peripheral() = default;
    virtual void initialize() { assert(!"Virtual base function called"); };
    virtual void update() { assert(!"Virtual base function called"); };
    virtual void safe_shutdown(bool critical) { assert(!"Virtual base function called"); };

    bool needs_update() { return is_updateable() && _timer->expired(); }
    bool is_updateable() const { return _timer != std::nullopt; }
    time_ms update_interval() const { return _timer->sampling_interval(); }

private:
    std::optional<IntervalTimer> _timer;
};

} // namespace kaskas::io