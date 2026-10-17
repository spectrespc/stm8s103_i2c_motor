# STM8S103F3P6 Dual Stepper Motor Controller (I²C Slave)

## 1. Quick Start
- Requirements: SDCC ≥ 4.0, STM8-SPL-SDCC in ~/STM8-SPL-SDCC, stm8flash, ST-LINK/v2
- Building: `make clean && make` → build/firmware.ihx
- Flashing: `make flash` (or `stm8flash -c stlinkv2 -p stm8s103f3 -s flash -w build/firmware.ihx`)
- Testing: `i2cdetect -y 7` (should show 0x60)

## 2. Features
- I²C slave addr 0x60, 100 kHz standard mode
- 2 stepper motors: focus + zoom (STEP/DIR/ENABLE, active-low EN)
- TIM1 PWM step generation, 1 ms system tick (TIM4)
- IWDG watchdog ~500 ms, fed in main loop + TIM4 ISR
- EEPROM position persistence with CRC16
- Limit switch inputs with 5 ms debounce
- I²C error recovery (AF/BERR/OVR → software reset, 2 s timeout)
- HSI 16 MHz, no external crystal

## 3. Hardware Requirements
- **External pull-ups on PB4/PB5 (I²C): 4.7 kΩ to VDD — MANDATORY** (true open-drain, no internal pull-ups)
- External pull-ups on PA1/PA2 (limit switches): 10 kΩ to VDD recommended
- Stepper drivers: any STEP/DIR driver (A4988, DRV8825, TMC2208)
- Power: 3.3V or 5V (STM8S103 tolerates 2.95–5.5V)
- SWIM programming: connect PD1 to ST-LINK SWIM, NRST to ST-LINK RST

## 4. Pin Map (TSSOP20)
```
     ┌─────────────────┐
     │  PB5  PB4  PB3 │
     │  PC1  PC2  PC3 │
     │  PC4  PC5  PC6 │
     │  PC7  PD1  PD2 │
     │  PA1  PA2  PD3 │
     │  PA3  PD4  PD5 │
     │  PD6  PD7  NRST │
     └─────────────────┘

     Pin 1: PD4 (LED_STATUS)
     Pin 2: PD3
     Pin 3: PD2
     Pin 4: PD1 (ZOOM_EN) - SWIM pin
     Pin 5: PA3
     Pin 6: PA2 (ZOOM_LIMIT)
     Pin 7: PA1 (FOCUS_LIMIT)
     Pin 8: PB1
     Pin 9: PB2
     Pin 10: PB3
     Pin 11: PB4 (I2C_SCL)
     Pin 12: PB5 (I2C_SDA)
     Pin 13: PC3 (FOCUS_STEP)
     Pin 14: PC4 (FOCUS_DIR)
     Pin 15: PC5 (FOCUS_EN)
     Pin 16: PC6 (ZOOM_STEP)
     Pin 17: PC7 (ZOOM_DIR)
     Pin 18: PD1 (ZOOM_EN)
     Pin 19: NRST
     Pin 20: VDD
```

## 5. I²C Protocol Summary
```
Commands:
- 0x01: Move motor (int16 steps)
- 0x02: Set speed (uint16 steps/sec)
- 0x03: Stop motor
- 0x04: Get position (int32)
- 0x05: Get status (uint8)
- 0x06: Reset controller
- 0x07: Calibrate (move to limit at speed)
- 0xFF: Get firmware version (3 bytes)
```

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
├── i2c/    i2c_slave.c/h
└── motor/  stepper.c/h, eeprom.c/h
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
- TIM1 shared ARR: motors run at same speed or sequentially
- No acceleration ramp (v2)
- PD1 conflicts with SWIM during programming
- EEPROM endurance: writes throttled to ≤ 1 Hz

## 9. Troubleshooting
- SWIM error 0x04: check power, SWIM connection, NRST
- I²C not detected: check external pull-ups on PB4/PB5
- No step pulses: check TIM1_CtrlPWMOutputs call, MOE bit
- WDT reset loop: ensure IWDG_ReloadCounter() called