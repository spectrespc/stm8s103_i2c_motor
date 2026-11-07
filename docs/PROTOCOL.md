# I²C Protocol Specification — STM8S103 Motor Controller

> 🇷🇺 [Русская версия](PROTOCOL.ru.md)

## Bus Parameters
- Speed: Standard mode (100 kHz)
- Slave address: 0x60 (7-bit)
- Write address: 0xC0, Read address: 0xC1
- External pull-ups: 4.7 kΩ on SDA/SCL

## Transaction Format
- **Write-only**: `[START][0xC0][CMD][MOTOR][PARAM...][STOP]`
- **Write+Read**: `[START][0xC0][CMD][MOTOR][REPEATED START][0xC1][DATA...][!ACK][STOP]`

## Command Reference

| Cmd | Write payload | Read payload | Description |
|-------|-------------|--------------|-------------|
| 0x01 | [cmd][motor][steps_hi][steps_lo] | — | Move motor by N steps (int16 BE, two's complement) |
| 0x02 | [cmd][motor][spd_hi][spd_lo] | — | Set speed (uint16 BE, steps/sec, max 5000) |
| 0x03 | [cmd][motor] | — | Stop motor |
| 0x04 | [cmd][motor] | [pos_0..pos_3] | Request current position (int32 BE) |
| 0x05 | [cmd][motor] | [status] | Request status (0=IDLE, 1=RUNNING, 2=ERROR) |
| 0x06 | [cmd] | — | Software reset controller |
| 0x07 | [cmd] | — | Enter the I2C bootloader (OKO fork) |
| 0x08 | [cmd] | [role][maj][min][pat] | Get role (0=app, 1=boot) and version (shared with the bootloader) |
| 0x09 | [cmd][motor][pos_0..pos_3] | — | Force the logical position (int32 BE) without moving; ignored while RUNNING (2.0.2+) |
| 0xFF | [cmd] | [maj][min][pat] | Get firmware version (3 bytes; application only) |

## Detailed Command Examples

### Version Request
```bash
i2ctransfer -y 3 w1@0x60 0xFF r3
# Response: 0x02 0x00 0x02 (version 2.0.2)
```

### Set Speed
```bash
# Set focus motor speed to 500 steps/sec
i2ctransfer -y 3 w4@0x60 0x02 0x00 0x01 0xF4
```

### Move Motor
```bash
# Move focus motor +100 steps
i2ctransfer -y 3 w4@0x60 0x01 0x00 0x00 0x64
```

### Get Position
```bash
# Get focus motor position
i2ctransfer -y 3 w2@0x60 0x04 0x00 r4
# Response: [pos_0..pos_3] (int32 BE)
```

### Get Status
```bash
# Get zoom motor status
i2ctransfer -y 3 w2@0x60 0x05 0x01 r1
# Response: 0x00 (IDLE), 0x01 (RUNNING) or 0x02 (ERROR)
```

### Stop Motor
```bash
# Stop zoom motor
i2ctransfer -y 3 w2@0x60 0x03 0x01
```

### Software Reset
```bash
# Reset controller
i2ctransfer -y 3 w1@0x60 0x06
```

### Set Position (zero the scale)
```bash
# Force the focus position to 0 without driving the motor
i2ctransfer -y 3 w6@0x60 0x09 0x00 0x00 0x00 0x00 0x00
# Verify the readback with 0x04; EEPROM persistence follows within ~1 s
```

**Known defect (KI-042):** on 2.0.2/2.1.1 a move that ends below zero leaves
the logical position with a corrupted top byte (`-1` reads as `0xFBFFFFFF`),
although the motor travels the commanded number of steps. Do not use a
negative `GET_POS` value for control; send `SET_POS(0)` after homing instead.

## Error Handling
- Unknown commands are ignored (no NACK); 0x07 is ENTER_BOOT in this fork.
- I2C errors (AF/BERR/OVR) → automatic module reset
- Timeout 2 s without activity → module reset

## OKO bootloader

This fork adds command `0x07` (ENTER_BOOT): the application stores `0x42` in
EEPROM `0x4010` and lets the IWDG reset the MCU. The bootloader at
`0x8000–0x89FF` answers commands `0x10–0x14`; see `BOOTLOADER.md` for the
frames, CRC and the `0x8A00`/`0x8A10` application layout.
