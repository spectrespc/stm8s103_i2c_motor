# STM8S103F3P6 Dual Stepper Motor Controller (I²C Slave)

## 1. Quick Start
- Requirements: SDCC ≥ 4.0, STM8-SPL-SDCC in ~/STM8-SPL-SDCC, stm8flash, ST-LINK/v2
- Building: `make clean && make` → build/firmware.ihx
- Flashing: `make flash` (or `stm8flash -c stlinkv2 -p stm8s103f3 -s flash -w build/firmware.ihx`)
- Testing: `i2cdetect -y 7` (should show 0x60)

## 2. Features
- I²C slave addr 0x60, 100 kHz standard mode
- 2 bipolar stepper motors: focus + zoom, driven directly by two DRV8833
  H-bridges (full-step, both coils energised)
- Step generation by a 1 ms DDA in the TIM4 ISR (1–5000 steps/sec), no STEP/DIR
- IWDG watchdog ~500 ms, fed in main loop + TIM4 ISR
- EEPROM position persistence with CRC16
- nFAULT monitoring (overcurrent/thermal/UVLO) → latched ERROR, motor coasted
- I²C error recovery (AF/BERR/OVR → software reset, 2 s timeout)
- HSI 16 MHz, no external crystal

## 3. Hardware Requirements
- **External pull-ups on PB4/PB5 (I²C): 4.7 kΩ to VDD — MANDATORY** (true open-drain, no internal pull-ups)
- Stepper drivers: 2× DRV8833 (one per motor), plus a 0.01 µF VCP cap and a
  10 µF VM bulk cap per chip
- Power: 3.3V or 5V (STM8S103 tolerates 2.95–5.5V); DRV8833 VM 2.7–10.8V
- SWIM programming: connect PD1 to ST-LINK SWIM, NRST to ST-LINK RST

## 4. Pin Map
The authoritative TSSOP20 pin map and project signal assignment is in
[docs/HARDWARE.md](docs/HARDWARE.md). Note that PB0–PB3 are not bonded on the
TSSOP20 package, so all coil/nSLEEP/nFAULT signals use ports A, C and D.

## 5. I²C Protocol Summary
Full command reference (payloads, byte order, response sizes) is in
[docs/PROTOCOL.md](docs/PROTOCOL.md). Command codes are 0x01–0x06 and 0xFF
(0x07 calibrate was removed in 2.0.0).

## 6. Building from Source
This project requires:
- SDCC compiler (version 4.0 or newer)
- STM8-SPL-SDCC library in ~/STM8-SPL-SDCC directory
- ST-LINK/v2 programmer for flashing

Sources are organised into functional modules:
```
src/
├── core/   main.c, main.h, stm8s_conf.h
├── hal/    hw_config.c/h, stm8s_it.c/h
├── i2c/    i2c_slave.c/h, i2c_proto.h (shared command table)
└── motor/  stepper.c/h, stepper_logic.h (pure DDA/phase), eeprom.c/h
```

To build the firmware (run from the repository root):
```
make clean && make
```

The firmware will be generated as build/firmware.ihx. All intermediate
artifacts (.rel, .asm, .lst, .map, ...) are written to build/ and are not
tracked by git. Host-side protocol tests live in tests/ and are run with
`make -C tests test`.

## 7. Testing
Basic testing commands:
```bash
# Check I2C device is present
i2cdetect -y 7

# Get firmware version
i2ctransfer -y 7 w1@0x60 0xFF r3

# Move focus motor +100 steps
i2ctransfer -y 7 w4@0x60 0x02 0x00 0x01 0xF4  # set speed
i2ctransfer -y 7 w4@0x60 0x01 0x00 0x00 0x64   # move

# Get focus motor position
i2ctransfer -y 7 w2@0x60 0x04 0x00 r4

# Get focus motor status
i2ctransfer -y 7 w2@0x60 0x05 0x00 r1
```

## 8. Known Limitations
- Open-loop: position is software-only (no encoder feedback). A blocked/skipped
  motor is detected only via nFAULT (overcurrent), not by position loss.
- No acceleration ramp
- A move is ignored until a speed has been set (`motorctl speed …` first)
- EEPROM endurance: writes throttled to ≤ 1 Hz and skipped while a motor runs

## 9. Troubleshooting
- SWIM error 0x04: check power, SWIM connection, NRST (PD1 must be unloaded)
- I²C not detected: check external pull-ups on PB4/PB5
- Motor does not turn: set a speed first, then move; check DRV8833 nSLEEP wiring
- Motor instantly reports ERROR: check for DRV8833 nFAULT (short/overcurrent)
- WDT reset loop: ensure IWDG_ReloadCounter() called