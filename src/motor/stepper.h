#ifndef STEPPER_H
#define STEPPER_H

#include <stdint.h>

/* The stepper pins are SPL GPIO pointers in the firmware. The host-side tests
   include this header only transitively (via main.h) and never instantiate a
   stepper_t, so a stub type keeps the header host-compilable without pulling in
   the whole SPL. The firmware build defines STM8S103 (see the root Makefile). */
#ifndef STM8S103
typedef void GPIO_TypeDef;
#else
#include "stm8s.h"
#endif

/* Motor state (reported by command 0x05). */
#define STATUS_IDLE      0
#define STATUS_RUNNING   1
#define STATUS_ERROR     2

typedef struct {
    int32_t  position;       /* current position (steps)               */
    int32_t  target;         /* target position (steps)                */
    uint16_t speed;          /* speed (steps/sec, 1..MAX_STEP_SPEED)   */
    uint8_t  state;          /* STATUS_IDLE / STATUS_RUNNING / ERROR   */
    uint8_t  phase;          /* current full-step phase (0..3)         */
    int16_t  step_acc;       /* DDA accumulator (integer steps/sec)    */
    uint8_t  wake_ticks;     /* ticks left to let nSLEEP settle (see c) */

    /* Coil pins (DRV8833 AIN1/AIN2/BIN1/BIN2) */
    GPIO_TypeDef* ain1_port; uint8_t ain1_pin;
    GPIO_TypeDef* ain2_port; uint8_t ain2_pin;
    GPIO_TypeDef* bin1_port; uint8_t bin1_pin;
    GPIO_TypeDef* bin2_port; uint8_t bin2_pin;

    /* Driver controls */
    GPIO_TypeDef* nsleep_port; uint8_t nsleep_pin;
    GPIO_TypeDef* nfault_port; uint8_t nfault_pin;
} stepper_t;

extern stepper_t steppers[2];

void stepper_init(void);
void stepper_move(uint8_t motor, int16_t steps);
void stepper_stop(uint8_t motor);
void stepper_set_speed(uint8_t motor, uint16_t speed);
void stepper_tick(void);

#endif