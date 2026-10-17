#ifndef I2C_SLAVE_H
#define I2C_SLAVE_H

#include <stdint.h>

#define I2C_RX_BUF_SIZE 8
#define I2C_TX_BUF_SIZE 8

extern volatile uint8_t i2c_rx_buf[I2C_RX_BUF_SIZE];
extern volatile uint8_t i2c_tx_buf[I2C_TX_BUF_SIZE];
extern volatile uint8_t i2c_rx_idx;
extern volatile uint8_t i2c_tx_idx;
extern volatile uint8_t i2c_rx_len;
extern volatile uint8_t i2c_state;
extern volatile uint32_t i2c_last_activity;

void i2c_slave_reset(void);
void i2c_process_command(void);
void i2c_prepare_read_response(void);
uint8_t i2c_check_timeout(void);

#endif
