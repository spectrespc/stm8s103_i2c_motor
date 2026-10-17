#ifndef I2C_PROTO_H
#define I2C_PROTO_H

#include <stdint.h>

/* I2C command codes — single source of truth shared by the firmware
   (src/i2c/i2c_slave.c) and the host tests (tests/test_i2c_protocol.c). */
#define I2C_CMD_MOVE         0x01
#define I2C_CMD_SET_SPEED    0x02
#define I2C_CMD_STOP         0x03
#define I2C_CMD_GET_POS      0x04
#define I2C_CMD_GET_STATUS   0x05
#define I2C_CMD_RESET        0x06
#define I2C_CMD_GET_VERSION  0xFF

/* X-macro table: code, minimum number of received bytes.
   Kept in one place so the firmware and the tests cannot drift apart. */
#define I2C_COMMAND_LENGTHS(X)   \
    X(I2C_CMD_MOVE,        4)    \
    X(I2C_CMD_SET_SPEED,   4)    \
    X(I2C_CMD_STOP,        2)    \
    X(I2C_CMD_GET_POS,     2)    \
    X(I2C_CMD_GET_STATUS,  2)    \
    X(I2C_CMD_RESET,       1)    \
    X(I2C_CMD_GET_VERSION, 1)

/* Minimum number of received bytes required for a valid command.
   An unknown command falls back to 1 (only the command byte). */
static uint8_t i2c_required_len(uint8_t cmd)
{
    switch (cmd)
    {
#define I2C_LEN_CASE(code, len) case (code): return (len);
        I2C_COMMAND_LENGTHS(I2C_LEN_CASE)
#undef I2C_LEN_CASE
        default: return 1;
    }
}

#endif /* I2C_PROTO_H */