#pragma once

namespace kaskas {

// ComponentCheck: evaluates whether a change in state should be made
// ComponentFollowUp: updates all sensors/actuators of component

enum class Events {
    WakeUp, //
    UserButtonCheck,
    OutOfWater,
    VentilationFollowUp,
    VentilationStart,
    VentilationStop,
    HeatingAutoTuneHeater, // heating element temperature with respect to surface probe
    HeatingAutoTuneClimateControl, // heating element temperature setpoint with respect to climate temperature
    HeatingFollowUp,
    HeatingCycleCheck,
    HeatingCycleStart,
    HeatingCycleStop,
    WaterLevelCheck,
    WaterInjectCheck,
    WaterInjectStart,
    WaterInjectFollowUp,
    WaterInjectStop,
    LightCycleStart,
    LightCycleEnd,
    VioletSpectrumTurnOn,
    VioletSpectrumTurnOff,
    BroadSpectrumTurnOn,
    BroadSpectrumTurnOff,
    Size
};
}; // namespace kaskas