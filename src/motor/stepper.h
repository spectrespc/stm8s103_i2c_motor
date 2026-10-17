#ifndef STEPPER_H
#define STEPPER_H

#include <stdint.h>

typedef struct {
    int32_t position;
    int32_t target;
    uint16_t speed;
    uint8_t state;
    uint8_t dir_pin;
    uint8_t en_pin;
    uint8_t limit_pin;
    uint8_t limit_debounce;
    uint8_t limit_stable;
    uint8_t tim1_channel;
    uint8_t calibrating;
} stepper_t;

#define STATUS_IDLE   0
#define STATUS_RUNNING   1
#define STATUS_ERROR   2
#define STATUS_LIMIT   3

extern stepper_t steppers[2];

void stepper_init(void);
void stepper_move(uint8_t motor, int16_t steps);
void stepper_stop(uint8_t motor);
void stepper_set_speed(uint8_t motor, uint16_t speed);
void stepper_calibrate(uint8_t motor, uint16_t speed);
void stepper_tick(void);

#endif