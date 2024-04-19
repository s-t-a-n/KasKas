#pragma once

namespace kaskas {

enum class Events {
    ClockSync, //
    OutOfWater, //
    WaterLevelCheck, //
    WaterInject, //
    LightCycleStart, //
    LightCycleEnd, //
    VioletSpectrumTurnOn, //
    VioletSpectrumTurnOff, //
    BroadSpectrumTurnOn, //
    BroadSpectrumTurnOff, //
    Size
};
};