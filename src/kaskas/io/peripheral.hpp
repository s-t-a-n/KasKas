#pragma once

#include <spine/core/time.hpp>
#include <spine/core/timers.hpp>
#include <spine/filter/filterstack.hpp>
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
    time_ms update_interval() const {
        assert(is_updateable());
        return is_updateable() ? _timer->sampling_interval() : time_ms(0);
    }

private:
    std::optional<IntervalTimer> _timer;
};

class FilteredPeripheral : public Peripheral {
public:
    using Filter = spn::filter::Filter<double>;

    // using Peripheral::Peripheral;
    explicit FilteredPeripheral(const std::optional<time_ms>& sampling_interval = std::nullopt,
                                size_t number_of_filters = 0)
        : Peripheral(sampling_interval), _fs(number_of_filters) {}

    void attach_filter(std::unique_ptr<Filter> filter) {
        assert(_fs.max_number_of_filters() > 0);
        _fs.attach_filter(std::move(filter));
    }

protected:
    spn::filter::Stack<double> _fs;
};

} // namespace kaskas::io
