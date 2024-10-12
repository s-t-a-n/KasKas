#pragma once

#include <spine/filter/filterstack.hpp>
#include <spine/structure/array.hpp>
#include <spine/structure/time/timers.hpp>
#include <spine/structure/units/si.hpp>

#include <optional>

namespace kaskas::io {

/// Encapsalates a single hardware peripheral
class Peripheral {
public:
    explicit Peripheral(const std::optional<time_ms>& sampling_interval = std::nullopt)
        : _timer(sampling_interval ? std::make_optional(IntervalTimer(sampling_interval.value())) : std::nullopt) {}

    virtual ~Peripheral() = default;
    virtual void initialize() { spn_assert(!"Virtual base function called"); };
    virtual void update() { spn_assert(!"Virtual base function called"); };
    virtual void safe_shutdown(bool critical) { spn_assert(!"Virtual base function called"); };

    bool needs_update() { return is_updateable() && _timer->expired(); }
    bool is_updateable() const { return _timer != std::nullopt; }
    time_ms update_interval() const {
        spn_expect(is_updateable());
        return is_updateable() ? _timer->interval() : time_ms(0);
    }
    time_ms time_until_next_update() const {
        spn_expect(is_updateable());
        return is_updateable() ? _timer->time_until_next() : time_ms(0);
    }

private:
    using IntervalTimer = spn::structure::time::IntervalTimer;

    std::optional<IntervalTimer> _timer;
};

class FilteredPeripheral : public Peripheral {
public:
    using Filter = spn::filter::Filter<double>;

    explicit FilteredPeripheral(const std::optional<time_ms>& sampling_interval = std::nullopt,
                                size_t number_of_filters = 0)
        : Peripheral(sampling_interval), _fs(number_of_filters) {}

    void attach_filter(std::unique_ptr<Filter> filter) {
        spn_expect(_fs.filter_slots_occupied() < _fs.filter_slots());
        _fs.attach_filter(std::move(filter));
    }

protected:
    spn::filter::Stack<double> _fs;
};

} // namespace kaskas::io
