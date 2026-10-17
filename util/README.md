# motorctl - I2C Utility for STM8S103 Motor Controller

This utility allows controlling the STM8S103 dual stepper motor controller over I2C.

## Building

```bash
cd util
make
```

## Installation

```bash
make install
```

This will install the binary to `/usr/local/bin/motorctl`.

## Usage

```
Usage: motorctl [OPTIONS] COMMAND [ARGS]
Options:
  -b BUS    I2C bus number (default: 7)
  -a ADDR  I2C address (default: 0x60)
  -h        Show this help message

Commands:
  version        Get firmware version
  speed <motor> <speed>  Set motor speed
  move <motor> <steps>  Move motor by steps
  position <motor>    Get motor position
  status <motor>  Get motor status
  stop <motor>    Stop motor
  reset          Reset controller

Motor names: focus, zoom
```

## Examples

Get firmware version:
```bash
motorctl version
```

Set focus motor speed to 500 steps/sec:
```bash
motorctl speed focus 500
```

Move zoom motor by 100 steps:
```bash
motorctl move zoom 100
```

Get focus motor position:
```bash
motorctl position focus
```

Get zoom motor status:
```bash
motorctl status zoom
```

Stop focus motor:
```bash
motorctl stop focus
```

Reset controller:
```bash
motorctl reset
```

## Requirements

- Linux system with I2C support
- I2C permissions (typically requires root or i2c group membership)
- Connected STM8S103 motor controller at I2C address 0x60