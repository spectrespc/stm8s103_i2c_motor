# Hardware Connection Guide

> 🇷🇺 [Русская версия](HARDWARE.ru.md)

## Board / MCU
- STM8S103F3P6, TSSOP20 package
- No external crystal — HSI 16 MHz
- PA1/PA2 are used as GPIO (OSCIN/OSCOUT unused)

## Package constraint
On the **TSSOP20** package only **PB4/PB5** of port B are bonded out; **PB0–PB3
do not exist** on this package. All coil / nSLEEP / nFAULT signals therefore use
ports A, C and D. (PB0–PB3 appear in the STM8S103K3 LQFP32 pinout only.)

## Pinout (TSSOP20)

| Pin | Name | Project signal | Notes |
|-----|------|----------------|-------|
| 1  | PD4 | LED_STATUS     | Status LED |
| 2  | PD5 | ZOOM_AIN1      | Zoom DRV8833, coil A input 1 |
| 3  | PD6 | ZOOM_AIN2      | Zoom DRV8833, coil A input 2 |
| 4  | NRST| RESET          | ST-LINK RST |
| 5  | PA1 | ZOOM_nSLEEP    | Zoom driver enable (HIGH = active) |
| 6  | PA2 | ZOOM_nFAULT    | Zoom driver fault (open-drain, LOW = fault, pull-up) |
| 7  | VSS | GND            | |
| 8  | VCAP| —              | 1 µF to VSS (internal regulator) |
| 9  | VDD | Power          | 2.95–5.5 V |
| 10 | PA3 | ZOOM_BIN1      | Zoom DRV8833, coil B input 1 |
| 11 | PB5 | I2C_SDA        | True open-drain, external pull-up |
| 12 | PB4 | I2C_SCL        | True open-drain, external pull-up |
| 13 | PC3 | FOCUS_AIN1     | Focus DRV8833, coil A input 1 |
| 14 | PC4 | FOCUS_AIN2     | Focus DRV8833, coil A input 2 |
| 15 | PC5 | FOCUS_BIN1     | Focus DRV8833, coil B input 1 |
| 16 | PC6 | FOCUS_BIN2     | Focus DRV8833, coil B input 2 |
| 17 | PC7 | ZOOM_BIN2      | Zoom DRV8833, coil B input 2 |
| 18 | PD1 | SWIM           | Left free for programming (not driven) |
| 19 | PD2 | FOCUS_nFAULT   | Focus driver fault (open-drain, LOW = fault, pull-up) |
| 20 | PD3 | FOCUS_nSLEEP   | Focus driver enable (HIGH = active) |

## External Components (MANDATORY)
| Component | Value | Pins | Notes |
|---------------|---------|------|--------|
| I²C pull-up | 4.7 kΩ | PB4→VDD, PB5→VDD | TRUE open-drain. Without them I²C won't work. |
| nFAULT pull-up | internal | PA2, PD2 | Configured as input with internal pull-up |
| Decoupling | 100 nF | VDD→VSS | Near each VDD pin |
| Bulk capacitor | 10 µF | VDD→VSS | |
| VCAP | 1 µF | VCAP→VSS | Required for internal regulator |
| DRV8833 VCP cap | 0.01 µF | VCP→VM | Charge pump, one per DRV8833 |
| DRV8833 bulk | 10 µF | VM→GND | One per DRV8833, close to the VM pin |

## Stepper Driver Connection (DRV8833)
Two DRV8833 chips, one per bipolar stepper, wired for full-step (both coils
energised). The MCU sequences the four coil inputs directly — there is no
STEP/DIR interface.

```
Focus DRV8833 #1:
  PC3 -> AIN1     PC4 -> AIN2     (coil A)
  PC5 -> BIN1     PC6 -> BIN2     (coil B)
  PD3 -> nSLEEP   PD2 -> nFAULT
  AOUT1/AOUT2 -> coil A, BOUT1/BOUT2 -> coil B

Zoom DRV8833 #2:
  PD5 -> AIN1     PD6 -> AIN2     (coil A)
  PA3 -> BIN1     PC7 -> BIN2     (coil B)
  PA1 -> nSLEEP   PA2 -> nFAULT
  AOUT1/AOUT2 -> coil A, BOUT1/BOUT2 -> coil B
```

Per H-bridge input coding (DRV8833): `{IN1,IN2}` = `{1,0}` forward current,
`{0,1}` reverse current, `{0,0}` coast, `{1,1}` brake. The firmware coasts
(`{0,0}`) when idle/stopped.

- **nSLEEP**: HIGH enables the driver, LOW puts it in low-power sleep. The
  firmware keeps it LOW while idle and raises it only during a move, so the
  coils are never energised at rest.
- **nFAULT**: open-drain, pulled LOW on overcurrent / thermal shutdown / UVLO.
  It is checked every 1 ms; a low level latches `STATUS_ERROR` on that motor.

## SWIM Programming Header
- PD1 (pin 18) → ST-LINK SWIM
- NRST (pin 4) → ST-LINK RST
- VDD, GND → power

PD1 is left in its reset state specifically so SWIM stays usable; do not load it
externally.

## Power Requirements
- STM8S103: ≤ 5 mA @ 16 MHz (idle, motors off)
- DRV8833 VM: 2.7–10.8 V, up to 1.5 A per bridge
- Keep the MCU supply (3.3 V/5 V) separate from VM where possible

## PCB Layout Notes
- I²C traces: keep short, avoid crosstalk
- Coil traces: keep the two coils of a motor together; watch VM/GND current paths
- VCAP and DRV8833 VCP/bulk caps: place as close as possible to their pins