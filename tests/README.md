# Tests

Host-side (Linux + gcc) unit tests for the I²C command handling logic.

## Running

From the repository root:

```bash
make -C tests test
```

or from this directory:

```bash
make test
```

The test binary prints a check/failure count and exits non-zero if any check
fails.

## Scope

The firmware protocol code (`src/i2c/i2c_slave.c`) touches STM8 SPL registers
(`I2C->SR1`, `GPIO_*`, `TIM1_*`, `sim()`/`rim()`) and cannot be linked on the
host. `test_i2c_protocol.c` therefore exercises the same pure logic using
mirrored helpers:

- command length table (`i2c_required_len`)
- big-endian parsing of 16/32-bit fields
- speed clamping (`stepper_set_speed` bounds)
- motor index validation (`motor < 2`)
- int32 position-accumulation overflow guard (`stepper_move`)

Keep the helpers in `test_i2c_protocol.c` in sync with the firmware when the
protocol changes.

## Files

- `Makefile` — builds and runs the tests (not used by the firmware build)
- `test_i2c_protocol.c` — the tests