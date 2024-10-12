# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](http://keepachangelog.com/en/1.0.0/)
and this project adheres to [Semantic Versioning](http://semver.org/spec/v2.0.0.html).

## [Unreleased]

-

<!--

### Added

### Changed

### Fixed

### Removed

-->

## [0.1.0] - 2024-10-12

### Added

- SHT31/DS18B20/DS3231: added sensor lockout exception for too many failed updates
- io/analogue output: Added `creep_stop()` which stops the hardware stack backend from trying to ‘creep’ towards the
  target.
- controller/heater: added dynamic gain to configured Kp for error's outside of heater's stable region, effectively
  making heater a lot more aggressive when not in stable temperature region

### Changed

- changed many DBG statements to LOG/WARN/ERROR statements, making use of Spine's new logging API
- debugging: uses Spine's new debugging API (`spn_assert` instead of `assert`)
- SHT31: simplified and improved driver middleware

### Fixed

- subsys/climatecontrol: Fixed missing parameter for temperature sensor to `ClimateControl` constructor. The index of
  the enum used to access the peripheral was zero to begin with, so not effect in terms of execution.

## [0.0.1] - 2024-09-23

### Added

- Initial release of **KasKas**.
- Automatic watering system based on soil moisture levels.
- Light control system to provide lighting from morning to evening.
- Heating system to maintain chamber temperature at 27°C during the day, cooling to 16°C at night.
- Ventilation system controlled by moisture levels.
- Access to runtime variables via UART RPC interface.
- Timeseries data available through UART RPC.
- Unified GPIO handling for sensors and actuators.
