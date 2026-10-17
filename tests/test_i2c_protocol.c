/*
 * Host-side unit tests for the I2C command handling logic.
 *
 * The protocol logic lives in firmware sources that are entangled with STM8 SPL
 * register access (I2C->SR1, GPIO_*, sim()/rim()) and therefore cannot be
 * linked on a Linux host. Shared definitions are taken straight from the
 * firmware headers; the remaining pure helpers mirror the firmware behaviour
 * and must be kept in sync when the protocol changes:
 *
 *   MAX_STEP_SPEED          -> src/core/main.h
 *   i2c_required_len()      -> src/i2c/i2c_proto.h (shared with the firmware)
 *   parse_be16/parse_be32   -> src/i2c/i2c_slave.c (i2c_process_command /
 *                              i2c_prepare_read_response)
 *   clamp_speed()           -> src/motor/stepper_logic.h (stepper_clamp_speed)
 *   overflow guard          -> src/motor/stepper_logic.h (stepper_move_target)
 *
 * Build and run with: make -C tests test
 */

#include <stdint.h>
#include <stdio.h>

#include "main.h"           /* MAX_STEP_SPEED: shared with the firmware */
#include "i2c_proto.h"      /* command codes + i2c_required_len(): shared */
#include "stepper_logic.h"  /* stepper_clamp_speed / stepper_move_target */

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

/* Thin wrapper over the shared helper so the assertions stay readable. */
static int target_would_overflow(int32_t position, int16_t steps)
{
    int32_t target;
    return stepper_move_target(position, steps, &target) != 0;
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

    /* Commands that carry only the motor index need 2 bytes */
    CHECK(i2c_required_len(0x03) == 2); /* stop       */
    CHECK(i2c_required_len(0x04) == 2); /* get pos    */
    CHECK(i2c_required_len(0x05) == 2); /* get status */

    /* Payload-free / unknown commands need 1 byte */
    CHECK(i2c_required_len(0x06) == 1); /* reset      */
    CHECK(i2c_required_len(0xFF) == 1); /* version    */
    CHECK(i2c_required_len(0x00) == 1); /* unknown    */
    CHECK(i2c_required_len(0x07) == 1); /* calibrate removed in 2.0.0 -> unknown */
    CHECK(i2c_required_len(0x08) == 1); /* unknown: adding a command must update
                                           i2c_proto.h and this test together */
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
    CHECK(stepper_clamp_speed(0, MAX_STEP_SPEED) == 1);              /* zero -> minimum */
    CHECK(stepper_clamp_speed(1, MAX_STEP_SPEED) == 1);
    CHECK(stepper_clamp_speed(MAX_STEP_SPEED, MAX_STEP_SPEED) == MAX_STEP_SPEED);
    CHECK(stepper_clamp_speed(MAX_STEP_SPEED + 1, MAX_STEP_SPEED) == MAX_STEP_SPEED);
    CHECK(stepper_clamp_speed(65535, MAX_STEP_SPEED) == MAX_STEP_SPEED);
}

/* Mirrors i2c_process_command()'s field extraction: the 16-bit payload of
   move/set-speed lives in bytes 2..3 of the frame
   [cmd][motor][hi][lo]. (A regression that read bytes 4..5 always got 0.) */
static int16_t extract_steps(const uint8_t *rx) { return parse_be16s(rx + 2); }
static uint16_t extract_speed(const uint8_t *rx) { return parse_be16u(rx + 2); }

/* Mirrors the shared motor-index guard (motor >= 2 is rejected). */
static int motor_valid(uint8_t m) { return m < 2; }

static void test_payload_offset(void)
{
    /* set speed frame: 0x02 0x00 0x01 0xF4 -> speed 500 at bytes 2..3 */
    uint8_t set_speed[6] = { 0x02, 0x00, 0x01, 0xF4, 0x00, 0x00 };
    CHECK(extract_speed(set_speed) == 500);

    /* move frame: 0x01 0x00 0x00 0x64 -> +100 steps */
    uint8_t move[6] = { 0x01, 0x00, 0x00, 0x64, 0x00, 0x00 };
    CHECK(extract_steps(move) == 100);

    /* negative move: 0x01 0x01 0xFF 0x9C -> -100 steps */
    uint8_t move_neg[6] = { 0x01, 0x01, 0xFF, 0x9C, 0x00, 0x00 };
    CHECK(extract_steps(move_neg) == -100);
}

static void test_motor_index(void)
{
    CHECK(motor_valid(0) == 1); /* motor 0 valid */
    CHECK(motor_valid(1) == 1); /* motor 1 valid */
    CHECK(motor_valid(2) == 0); /* motor 2 rejected */
    CHECK(motor_valid(3) == 0); /* motor 3 rejected */
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
    test_payload_offset();
    test_motor_index();
    test_int32_overflow_guard();

    printf("%d checks, %d failed\n", tests_run, tests_failed);
    if (tests_failed == 0)
        printf("ALL TESTS PASSED\n");
    return tests_failed == 0 ? 0 : 1;
}