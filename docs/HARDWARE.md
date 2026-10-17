# Hardware Connection Guide

## Board / MCU
- STM8S103F3P6, TSSOP20 package
- No external crystal — HSI 16 MHz
- PA1/PA2 repurposed as GPIO (OSCIN/OSCOUT unused)

## Pinout Diagram
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

## External Components (MANDATORY)
| Component | Value | Pins | Notes |
|---------------|---------|------|--------|
| I²C pull-up | 4.7 kΩ | PB4→VDD, PB5→VDD | TRUE open-drain. Without them I²C won't work. |
| Limit pull-up | 10 kΩ | PA1→VDD, PA2→VDD | Active-low limit switches |
| Decoupling | 100 nF | VDD→VSS | Near each VDD pin |
| Bulk capacitor | 10 µF | VDD→VSS | |
| VCAP | 1 µF | VCAP→VSS | Required for internal regulator |

## Stepper Driver Connection
```
Focus Motor:
PC3 → STEP
PC4 → DIR
PC5 → EN (active low)

Zoom Motor:
PC6 → STEP
PC7 → DIR
PD1 → EN (active low)
```

## SWIM Programming Header
- PD1 (pin 18) → ST-LINK SWIM
- NRST (pin 19) → ST-LINK RST
- VDD, GND → power

## Power Requirements
- STM8S103: ≤ 5 mA @ 16 MHz (idle, motors off)
- Stepper drivers: depends on motor (typ. 0.5–2A per motor)
- Total system: separate supplies for MCU (3.3V) and motors (12–24V) recommended

## PCB Layout Notes
- I²C traces: keep short, avoid crosstalk
- STEP/DIR traces: 10 cm max
- VCAP: place as close as possible to MCU