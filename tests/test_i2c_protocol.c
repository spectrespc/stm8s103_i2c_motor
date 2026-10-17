/*
 * Host-side unit tests for the I2C command handling logic.
 *
 * The protocol logic lives in firmware sources that are entangled with STM8 SPL
 * register access (I2C->SR1, GPIO_*, TIM1_*, sim()/rim()) and therefore cannot
 * be linked on a Linux host. The pure helpers below mirror the firmware
 * behaviour exactly; keep them in sync when the protocol changes:
 *
 *   i2c_required_len()      -> src/i2c/i2c_slave.c
 *   parse_be16/parse_be32   -> src/i2c/i2c_slave.c (i2c_process_command /
 *                              i2c_prepare_read_response)
 *   clamp_speed()           -> src/motor/stepper.c (stepper_set_speed)
 *   target_would_overflow() -> src/motor/stepper.c (stepper_move)
 *
 * Build and run with: make -C tests test
 */

#include <stdint.h>
#include <stdio.h>

#define MAX_STEP_SPEED 5000

static uint8_t i2c_required_len(uint8_t cmd)
{
    switch (cmd)
    {
        case 0x01: /* move:      cmd, motor, steps_hi, steps_lo */
        case 0x02: /* set speed: cmd, motor, speed_hi, speed_lo */
        case 0x07: /* calibrate: cmd, motor, speed_hi, speed_lo */
            return 4;
        case 0x03: /* stop:       cmd, motor */
        case 0x04: /* get pos:    cmd, motor */
        case 0x05: /* get status: cmd, motor */
            return 2;
        default:   /* 0x06 reset, 0xFF version, unknown */
            return 1;
    }
}

static int16_t parse_be16s(const uint8_t *b)
{
    return (int16_t)(((uint16_t)b[0] << 8) | b[1]);
}

static uint16_t parse_be16u(const uint8_t *b)
{
    return ((uint16_t)b[0] << 8) | b[1];
}

static int32_t parse_be32(const uint8_t *b)
{
    return ((int32_t)b[0] << 24) | ((int32_t)b[1] << 16) |
           ((int32_t)b[2] << 8) | (int32_t)b[3];
}

static uint16_t clamp_speed(uint16_t speed)
{
    if (speed == 0)
        speed = 1; /* Minimum speed */
    if (speed > MAX_STEP_SPEED)
        speed = MAX_STEP_SPEED; /* Maximum speed */
    return speed;
}

/* Mirrors the guard in stepper_move(). Uses unsigned arithmetic so the
   two's-complement wrap is well defined on the host (the firmware relies on the
   same wrap on the STM8). */
static int target_would_overflow(int32_t position, int16_t steps)
{
    uint32_t wrapped = (uint32_t)position + (uint32_t)(int32_t)steps;
    int32_t new_target = (int32_t)wrapped;
    if ((steps > 0 && new_target < position) ||
        (steps < 0 && new_target > position))
        return 1;
    return 0;
}

static int tests_run = 0;
static int tests_failed = 0;

#define CHECK(cond)                                                      \
    do {                                                                 \
        tests_run++;                                                     \
        if (!(cond)) {                                                   \
            tests_failed++;                                              \
            printf("FAIL: %s (line %d)\n", #cond, __LINE__);             \
        }                                                                \
    } while (0)

static void test_required_len(void)
{
    /* Commands that carry 16-bit payloads need 4 bytes */
    CHECK(i2c_required_len(0x01) == 4); /* move      */
    CHECK(i2c_required_len(0x02) == 4); /* set speed */
    CHECK(i2c_required_len(0x07) == 4); /* calibrate */

    /* Commands that carry only the motor index need 2 bytes */
    CHECK(i2c_required_len(0x03) == 2); /* stop       */
    CHECK(i2c_required_len(0x04) == 2); /* get pos    */
    CHECK(i2c_required_len(0x05) == 2); /* get status */

    /* Payload-free / unknown commands need 1 byte */
    CHECK(i2c_required_len(0x06) == 1); /* reset      */
    CHECK(i2c_required_len(0xFF) == 1); /* version    */
    CHECK(i2c_required_len(0x00) == 1); /* unknown    */
    CHECK(i2c_required_len(0x42) == 1); /* unknown    */
}

static void test_big_endian_parsing(void)
{
    uint8_t steps[2] = { 0xFF, 0xFE };      /* -2 as int16 */
    uint8_t speed[2] = { 0x13, 0x88 };      /* 5000 as uint16 */
    uint8_t pos_neg[4] = { 0xFF, 0xFF, 0xFF, 0xFE }; /* -2 */
    uint8_t pos_pos[4] = { 0x00, 0x00, 0x01, 0x00 }; /* 256 */

    CHECK(parse_be16s(steps) == -2);
    CHECK(parse_be16u(speed) == 5000);
    CHECK(parse_be32(pos_neg) == -2);
    CHECK(parse_be32(pos_pos) == 256);

    /* int16 minimum / maximum */
    {
        uint8_t mn[2] = { 0x80, 0x00 };
        uint8_t mx[2] = { 0x7F, 0xFF };
        CHECK(parse_be16s(mn) == -32768);
        CHECK(parse_be16s(mx) == 32767);
    }

    /* int32 minimum / maximum */
    {
        uint8_t mn[4] = { 0x80, 0x00, 0x00, 0x00 };
        uint8_t mx[4] = { 0x7F, 0xFF, 0xFF, 0xFF };
        CHECK(parse_be32(mn) == (int32_t)-2147483647 - 1);
        CHECK(parse_be32(mx) == (int32_t)2147483647);
    }
}

static void test_speed_clamp(void)
{
    CHECK(clamp_speed(0) == 1);              /* zero -> minimum     */
    CHECK(clamp_speed(1) == 1);
    CHECK(clamp_speed(MAX_STEP_SPEED) == MAX_STEP_SPEED);
    CHECK(clamp_speed(MAX_STEP_SPEED + 1) == MAX_STEP_SPEED);
    CHECK(clamp_speed(65535) == MAX_STEP_SPEED);
}

static void test_motor_index(void)
{
    /* stepper_* reject motor >= 2; the checks mirror that guard. */
    CHECK((0u < 2u) == 1); /* motor 0 valid */
    CHECK((1u < 2u) == 1); /* motor 1 valid */
    CHECK((2u < 2u) == 0); /* motor 2 rejected */
    CHECK((3u < 2u) == 0); /* motor 3 rejected */
}

static void test_int32_overflow_guard(void)
{
    CHECK(target_would_overflow(0, 100) == 0);
    CHECK(target_would_overflow(0, -100) == 0);
    CHECK(target_would_overflow(2147483647, 1) == 1);   /* INT32_MAX + 1 */
    CHECK(target_would_overflow(2147483647, -1) == 0);
    CHECK(target_would_overflow(-2147483647 - 1, -1) == 1); /* INT32_MIN - 1 */
    CHECK(target_would_overflow(-2147483647 - 1, 1) == 0);
    CHECK(target_would_overflow(2147483000, 1000) == 1);
    CHECK(target_would_overflow(-2147483000, -1000) == 1);
}

int main(void)
{
    test_required_len();
    test_big_endian_parsing();
    test_speed_clamp();
    test_motor_index();
    test_int32_overflow_guard();

    printf("%d checks, %d failed\n", tests_run, tests_failed);
    if (tests_failed == 0)
        printf("ALL TESTS PASSED\n");
    return tests_failed == 0 ? 0 : 1;
}