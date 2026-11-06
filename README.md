# STM8S103F3P6 Dual Stepper Motor Controller (I²C Slave)

> 🇷🇺 [Русская версия](README.ru.md)

> **Bootloader.** This is the STM8 motor controller used by OKO. Compared with
> the pre-bootloader layout (`3f36dcee4f05c02a48bcc48b8eea7e606fe7634e`), it
> adds an I2C bootloader, `oko-motor` and a new flash layout: boot
> `0x8000–0x89FF`, header `0x8A00`, application `0x8A10–0x9FFF`. The boot
> slot is 2.5 KiB because the measured boot image is 2412 bytes (the earlier
> 2 KiB assumption did not fit). Command 0x07 now means ENTER_BOOT; 0x08
> returns `[role, major, minor, patch]` in both roles; the bootloader owns
> 0x10–0x14. See [docs/BOOTLOADER.md](docs/BOOTLOADER.md).

## 1. Quick Start
- Requirements: SDCC ≥ 4.0 and STM8-SPL-SDCC; stm8flash and ST-LINK/v2 for
  the first factory write. The OKO SDK also provides a ready Docker image in
  `tools/stm8-sdcc` (Debian bookworm, SDCC 4.2).
- Building (from the repository root): `make` (or
  `make -f Makefile.stm8 all`) → `build/app-2.0.1.ihx` (I2C update),
  `build/factory-2.0.1.ihx` (ST-Link), `build/app-2.0.1.bin`; the version
  comes from `src/core/main.h`. Unversioned `build/app.ihx`,
  `build/factory.ihx` and `build/app.bin` are copies for ST-Link scripts, but
  `/api/flash` only accepts a versioned name (it derives the version from it).
  In the OKO SDK the same command runs in `tools/stm8-sdcc`:
  `docker run --rm -v "$PWD:/work" -w /work oko-stm8-sdcc make -f Makefile.stm8 all`
- Host utility: `make -C util` → `util/motorctl`.
- Flashing: first time `factory-2.0.1.ihx` over ST-Link/SWIM (see
  `docs/BOOTLOADER.md`); later `oko-motor flash build/app-2.0.1.ihx` over I2C
  or `/api/flash` in the OKO WebUI (upload the versioned name).
- Offline validation: `oko-motor check build/app-2.0.1.ihx` (no I2C, exit
  0/2); `oko-motor check FILE 2.0.1` also rejects a name whose version differs
  from the OKO8 header (exit 2). Leave the bootloader with
  `oko-motor boot-reset`.
- Testing: `i2cdetect -y 3` (the OKO board uses I2C3; adjust the bus for your
  host).

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
[docs/PROTOCOL.md](docs/PROTOCOL.md). Application command codes are 0x01–0x08
and 0xFF; 0x07 is ENTER_BOOT and 0x08 is GET_STATE (role + version) in this
fork. The bootloader protocol
(0x10–0x14) is in [docs/BOOTLOADER.md](docs/BOOTLOADER.md).

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

To build the firmware, run `make` in the repository root. The OKO SDK wraps
the same build in `tools/stm8-sdcc`:
```
docker run --rm -v "$PWD:/work" -w /work \
  oko-stm8-sdcc make -f Makefile.stm8 all
```

`Makefile.stm8` links the application at `0x8A10`, takes the version from
`src/core/main.h` and rejects an oversized image before writing it.
`tools/mkapp.py` writes the 16-byte header at `0x8A00` (magic `OKO8`, length,
version, CRC32) and the `factory-<version>.ihx` merge. `Makefile`
is dual-mode: standalone it includes `Makefile.stm8`, while the OKO SDK
(where `../Makefile.param` exists) uses it to cross-compile `oko-motor`. All
intermediate
artifacts (.rel, .asm, .lst, .map, ...) are written to build/ and are not
tracked by git. Host-side protocol tests live in tests/ and are run with
`make -C tests test`.

## 7. Testing
Basic testing commands:
```bash
# Check I2C device is present
i2cdetect -y 3

# Get firmware version
i2ctransfer -y 3 w1@0x60 0xFF r3

# Move focus motor +100 steps (bus 3 on the OKO board)
i2ctransfer -y 3 w4@0x60 0x02 0x00 0x01 0xF4  # set speed
i2ctransfer -y 3 w4@0x60 0x01 0x00 0x00 0x64   # move

# Get focus motor position
i2ctransfer -y 3 w2@0x60 0x04 0x00 r4

# Get focus motor status
i2ctransfer -y 3 w2@0x60 0x05 0x00 r1
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