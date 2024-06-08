#pragma once

namespace kaskas {

// ComponentCheck: evaluates whether a change in state should be made
// ComponentFollowUp: updates all sensors/actuators of component

enum class Events {
    WakeUp, //
    UIButtonCheck,
    UIWatchDog,
    SensorFollowUp,
    OutOfWater,
    VentilationFollowUp,
    VentilationStart,
    VentilationStop,
    HeatingAutoTune,
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