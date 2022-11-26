# Changelog
All notable changes to this project/module will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project/module adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

---
## V1.0.0 - dd.mm.yyyy

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

 - Configuration table check
    - lpf_fc != 0
    - pull connection mismatched...
 
 - Extensive measurement of NTC measurement:
    - NTC high side with single pull resistor
    - NTC low side with single pull resistor
    - NTC high side with both pull resistors

 - Extensive measurement of PT1000 measurement:
    - PT1000 high side with single pull resistor
    - PT1000 low side with single pull resistor
    - PT1000 high side with both pull resistors


---