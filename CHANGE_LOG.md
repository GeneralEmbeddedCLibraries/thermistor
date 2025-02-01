# Changelog
All notable changes to this project/module will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project/module adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

---
## V1.2.0 - 01.02.2025

### Added
 - Added API to reset LPF to wanted temperature

### Changed
 - Changed single pull resistor calculations, now reference is not needed anymore

### Todo
 - Thermistor status check testing
 - Adoption to analog module
 - Removing filter from thermistor and moving it into analog
 - Implementatiom for both pull-up and pull-down configuration is missing

---
## V1.1.0 - 09.11.2023

### Added
 - Compatibility check with filter module
 - Calculations to degree F and Kelvin
 - Added de-init function
 - Configuration table validy check

### Changed
 - Filter module version compatibility changes

### Todo
 - Thermistor status check testing
 - Power supply ripple compensation algorithm
 - Self-heating compensation algorithm
 - Adoption to analog module
 - Removing filter from thermistor and moving it into analog

---
## V1.0.0 - 08.12.2022

### Added
- Measurement and conversion of NTC with various HW configuration
- Power supply (Vcc) ripple compensation algorithm
- Status handling based on range (min, max)
    - Detection of short circuit
    - Detection of open circuit
- Added excel table with PT100, PT500 and PT1000 calculations
- Measurement and conversion of PT100, PT500 and PT1000 with various HW configuration
   - Software for PT calculations were tested with SIKA simulator

### Todo
 - Thermistor status check testing
 - Configuration table check
    - lpf_fc != 0
    - pull connection mismatched...
 - Power supply ripple compensation algorithm
 - Self-heating compensation algorithm

---