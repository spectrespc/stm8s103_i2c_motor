#ifndef MAIN_H
#define MAIN_H

#include <stdint.h>
#include "stepper.h"

#define I2C_SLAVE_ADDR      0x60
#define MAX_STEP_SPEED      5000
#define I2C_TIMEOUT_MS    2000

/* Shared with ISRs: must be volatile (and read atomically where 32-bit). */
extern volatile uint32_t system_ticks;
extern volatile uint32_t i2c_last_activity;
extern volatile uint8_t i2c_need_reset;
extern volatile uint8_t i2c_cmd_ready;
extern volatile uint8_t eeprom_dirty;
extern uint32_t eeprom_last_save_time;

void hw_clock_init(void);
void hw_gpio_init(void);
void hw_iwdg_init(void);
void hw_tim4_init(void);
void hw_i2c_init(void);
void hw_reset(void);

void i2c_process_command(void);
void i2c_slave_reset(void);
uint8_t i2c_check_timeout(void);
void i2c_isr_handler(void);

void eeprom_init(void);
void eeprom_periodic_save(void);

void stepper_init(void);
void stepper_move(uint8_t motor, int16_t steps);
void stepper_stop(uint8_t motor);
void stepper_set_speed(uint8_t motor, uint16_t speed);
void stepper_tick(void);

#endif