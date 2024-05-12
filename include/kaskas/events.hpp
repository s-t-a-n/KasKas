#pragma once

namespace kaskas {

enum class Events {
    WakeUp, //
    OutOfWater, //
    VentilationStart, //
    VentilationStop, //
    WaterLevelCheck, //
    WaterInjectCheck, //
    WaterInjectStart, //
    WaterInjectFollowUp, //
    WaterInjectStop, //
    LightCycleStart, //
    LightCycleEnd, //
    VioletSpectrumTurnOn, //
    VioletSpectrumTurnOff, //
    BroadSpectrumTurnOn, //
    BroadSpectrumTurnOff, //
    Size
};
};