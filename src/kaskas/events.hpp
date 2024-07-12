#pragma once

namespace kaskas {

// ComponentCheck: evaluates whether a change in state should be made
// ComponentFollowUp: updates all sensors/actuators of component

enum class Events {
    WakeUp, //
    ShutDown,
    UIButtonCheck,
    UIWatchDog,
    UIPromptFollowUp,
    SensorFollowUp,
    OutOfWater,
    VentilationFollowUp,
    VentilationCycleCheck,
    VentilationCycleStart,
    VentilationCycleStop,
    VentilationAutoTune,
    HeatingAutoTune,
    HeatingFollowUp,
    HeatingCycleCheck,
    HeatingCycleStart,
    HeatingCycleStop,
    WaterInjectCheck,
    WaterInjectEvaluateEffect,
    WaterInjectStart,
    WaterInjectFollowUp,
    WaterInjectStop,
    LightRedBlueSpectrumCycleCheck,
    LightRedBlueSpectrumCycleStart,
    LightRedBlueSpectrumCycleEnd,
    LightFullSpectrumCycleCheck,
    LightFullSpectrumCycleStart,
    LightFullSpectrumCycleEnd,
    DAQWarmedUp,
    DAQTainted,
    Size
};
}; // namespace kaskas