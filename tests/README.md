# Tests

> 🇷🇺 [Русская версия](README.ru.md)

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
(`I2C->SR1`, `GPIO_*`, `sim()`/`rim()`) and cannot be linked on the host.
`test_i2c_protocol.c` therefore exercises the same pure logic. Shared
definitions are taken directly from the firmware headers so they cannot drift:

- command codes and length table (`i2c_required_len`) — from `src/i2c/i2c_proto.h`
- speed limit (`MAX_STEP_SPEED`) — from `src/core/main.h`
- speed clamping and the move overflow guard — from `src/motor/stepper_logic.h`

`test_stepper_logic.c` covers the stepper core directly: the full-step
commutation table, the phase sequencer, the DDA step accumulation (including the
1 ms/1000-base arithmetic and the 5-steps/tick cap at 5000 steps/s), speed
bounds and the int32 target-overflow guard. The functions under test are the
same `static inline` helpers `src/motor/stepper.c` uses.

The remaining helper in `test_i2c_protocol.c` is mirrored and must be kept in
sync when the protocol changes:

- big-endian parsing of 16/32-bit fields and the command payload offset

## Files

- `Makefile` — builds and runs the tests (not used by the firmware build)
- `test_i2c_protocol.c` — I²C command/protocol tests
- `test_stepper_logic.c` — stepper DDA / commutation tests