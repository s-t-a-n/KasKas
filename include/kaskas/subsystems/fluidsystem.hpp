#pragma once

#include "kaskas/components/clock.hpp"
#include "kaskas/components/relay.hpp"
#include "kaskas/components/sensor.hpp"
#include "kaskas/events.hpp"

#include <spine/core/exception.hpp>
#include <spine/core/timers.hpp>
#include <spine/eventsystem/eventsystem.hpp>
#include <spine/filter/EWMA.hpp>
#include <spine/platform/hal.hpp>

#include <AH/STL/cstdint>

using spn::core::Exception;
using spn::core::time::Timer;
using spn::eventsystem::Event;
using spn::eventsystem::EventHandler;
using spn::filter::EWMA;

using Events = kaskas::Events;
using EventSystem = spn::core::EventSystem;

class Fluidsystem : public EventHandler {
public:
    using GroundMoistureSensorFilter = EWMA<double>;
    using GroundMoistureSensor = Sensor<AnalogueInput, GroundMoistureSensorFilter>;

    struct Config {
        Relay pump;
        GroundMoistureSensor ground_moisture_sensor;
        float ground_moisture_threshold; // value between 0.0 and 1.0 with 1.0 being the moistest of states the
                                         // sensor can pick up
        Interrupt interrupt;
        uint16_t dosis_ml;
        time_ms pump_check_interval;
        uint8_t check_interval_hours;
        float ml_calibration_factor;
        time_s timeout;
        time_s timeout_to_half_point;
    };

public:
    Fluidsystem(EventSystem* evsys, Config& cfg) : EventHandler(evsys), _cfg(cfg){};

    virtual void handle_event(Event* event) {
        switch (static_cast<Events>(event->id())) {
        case Events::OutOfWater: {
            //
            DBG("Fluidsystem: OutOfWater");
            break;
        }
        case Events::WaterLevelCheck: {
            // read waterlevel
            //            DBG("Fluidsystem: WaterLevelCheck");
            _cfg.ground_moisture_sensor.update();

            {
                char m[256];
                snprintf(m, sizeof(m), "Fluidsystem: Waterlevel: %f (threshold: %f)",
                         _cfg.ground_moisture_sensor.value(), _cfg.ground_moisture_threshold);
                DBG(m);
            }
            evsys()->schedule(evsys()->event(Events::WaterLevelCheck, time_s(10), Event::Data()));

            break;
        }
        case Events::WaterInjectCheck: {
            // should injection take place?
            DBG("Fluidsystem: WaterInjectCheck");
            {
                char m[256];
                snprintf(
                    m, sizeof(m),
                    "Fluidsystem: WaterInjectCheck: Moisture level at %f (threshold at %f, time since last injection: "
                    "%lli s)",
                    _cfg.ground_moisture_sensor.value(), _cfg.ground_moisture_threshold,
                    _pumptracker.time_since_last_injection().value / 1000);
                DBG(m);
            }
            const auto interval = _cfg.pump_check_interval * 60 * 60 * 1000;
            if (_cfg.ground_moisture_sensor.value() > _cfg.ground_moisture_threshold
                && _pumptracker.time_since_last_injection() > interval) {
                {
                    char m[256];
                    snprintf(
                        m, sizeof(m),
                        "Fluidsystem: Moisture level (%f) crossed threshold (%f) and %lli s since last, calling for "
                        "injection start.",
                        _cfg.ground_moisture_sensor.value(), _cfg.ground_moisture_threshold,
                        _pumptracker.time_since_last_injection().value / 1000);
                    DBG(m);
                }
                evsys()->schedule(evsys()->event(Events::WaterInjectStart, time_s(1), Event::Data()));
            }

            break;
        }
        case Events::WaterInjectStart: {
            DBG("Fluidsystem: WaterInjectStart");
            reset_interrupt_counter();
            _pumptracker.reset();
            attach_interrupt(); //
            _cfg.pump.set_state(DigitalState::ON);

            evsys()->schedule(evsys()->event(Events::WaterInjectFollowUp, _cfg.pump_check_interval, Event::Data()));
            break;
        }
        case Events::WaterInjectFollowUp: {
            //                        DBG("Fluidsystem: WaterInjectFollowUp");
            bool stop = false;
            _pumptracker.ml += ml_injected();

            constexpr bool no_reset = false;
            if (_pumptracker.pump_timer.timeSinceLast(no_reset) > time_ms(_cfg.timeout_to_half_point)
                && _pumptracker.ml < _cfg.dosis_ml / 2) {
                // not enough pumped within space of time
                DBG("Fluidsystem: WaterInjectFollowUp: not enough pumped within space of time");
                stop = true;
                evsys()->schedule(evsys()->event(Events::OutOfWater, time_ms(100), Event::Data()));
            }
            if (_pumptracker.ml >= _cfg.dosis_ml) {
                // reached dosis
                DBG("Fluidsystem: WaterInjectFollowUp: Satisfied inject request");
                stop = true;
            }

            if (stop) {
                DBG("Fluidsystem: Stopping injection");
                evsys()->trigger(evsys()->event(Events::WaterInjectStop, time_ms(0), Event::Data()));
            } else {
                evsys()->schedule(evsys()->event(Events::WaterInjectFollowUp, _cfg.pump_check_interval, Event::Data()));
            }
            break;
        }
        case Events::WaterInjectStop: {
            DBG("Fluidsystem: WaterInjectStop");
            detach_interrupt(); //
            _cfg.pump.set_state(DigitalState::OFF);

            char msg[512];
            snprintf(msg, sizeof(msg), "Fluidsystem: Pumped %lu ml in %lli ms (pumped in total: %lu L)",
                     _pumptracker.ml, _pumptracker.pump_timer.timeSinceLast(false).value,
                     _pumptracker.lifetime_pumped_ml);
            DBG(msg);
            break;
        }
        default: assert(!"event not handled"); break;
        }
    }
    void initialize() {
        _cfg.pump.initialize();
        _cfg.interrupt.initialize();

        evsys()->attach(Events::OutOfWater, this);
        evsys()->attach(Events::WaterLevelCheck, this);
        evsys()->attach(Events::WaterInjectCheck, this);
        evsys()->attach(Events::WaterInjectStart, this);
        evsys()->attach(Events::WaterInjectFollowUp, this);
        evsys()->attach(Events::WaterInjectStop, this);

        evsys()->schedule(evsys()->event(Events::WaterLevelCheck, time_s(10), Event::Data()));
        evsys()->schedule(
            evsys()->event(Events::WaterInjectCheck, time_s(_cfg.check_interval_hours * 60 * 60), Event::Data()));

        {
            char msg[512];
            snprintf(msg, sizeof(msg), "Fluidsystem: scheduling WaterInjectCheck event in %i hours.",
                     _cfg.check_interval_hours);
            DBG(msg);
        }
        //        _evsys.schedule(_evsys.event(Events::WaterInjectCheck, time_s(1), Event::Data()));
    }

private:
    uint32_t ml_injected() {
        detach_interrupt();
        const auto time_since_last_check = _pumptracker.followup_timer.timeSinceLast();

        // Because this loop may not complete in exactly _cfg.pump_check_interval intervals we calculate
        // the number of milliseconds that have passed since the last execution and use
        // that to scale the output. We also apply the calibrationFactor to scale the output
        // based on the number of pulses per second per units of measure (litres/minute in
        // this case) coming from the sensor.
        const auto pulse_count = interrupt_counter();
        reset_interrupt_counter();

        const auto flowrate_lm =
            ((_cfg.pump_check_interval.value / time_since_last_check.value) * pulse_count) / _cfg.ml_calibration_factor;

        // Divide the flow rate in litres/minute by 60 to determine how many litres have
        // passed through the sensor in this interval, then multiply by 1000 to
        // convert to millilitres.
        //        auto injected = (flowrate_lm / 60) * _cfg.pump_check_interval.value;
        auto injected = (flowrate_lm / 60) * _cfg.pump_check_interval.value;

        attach_interrupt();
        return injected;
    }

    // interrupt
    void attach_interrupt();
    void detach_interrupt();
    static uint32_t interrupt_counter();
    static void reset_interrupt_counter();

    Config _cfg;

    struct PumpTracker {
        uint32_t lifetime_pumped_ml = 0;
        uint32_t ml = 0;
        Timer pump_timer; // tracks time since start of pumping
        Timer followup_timer; // tracks time since last check

        time_ms time_since_last_injection() { return followup_timer.timeSinceLast(false); }
        void reset(bool reset_total = false) {
            pump_timer.reset();
            followup_timer.reset();
            if (reset_total) {
                lifetime_pumped_ml = 0;
            } else {
                lifetime_pumped_ml += ml;
            }
            ml = 0;
        }
    };

    PumpTracker _pumptracker;
};