#pragma once

// The names of the providers below are part of the prompt API
enum class DataProviders {
    CLIMATE_TEMP, //
    CLIMATE_HUMIDITY,
    AMBIENT_TEMP,
    CLOCK,
    SOIL_MOISTURE,
    SOIL_MOISTURE_SETPOINT,
    CLIMATE_FAN,
    HEATING_POWER,
    HEATING_SETPOINT,
    HEATING_SURFACE_TEMP,
    HEATING_SURFACE_FAN,
    HEATING_ELEMENT,
    REDBLUE_SPECTRUM,
    FULL_SPECTRUM,
    PUMP,
    SIZE
};
