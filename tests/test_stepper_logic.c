/*
 * Host-side unit tests for the stepper core logic (DDA + commutation).
 *
 * The logic under test lives in src/motor/stepper_logic.h, which is shared
 * verbatim with the firmware (src/motor/stepper.c includes it too), so these
 * tests exercise the real code rather than a copy.
 *
 * Build and run with: make -C tests test
 */

#include <stdint.h>
#include <stdio.h>

#include "main.h"          /* MAX_STEP_SPEED */
#include "stepper_logic.h" /* the code under test */

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

static void test_phase_table(void)
{
    uint8_t phase, coil, sum;

    /* Full-step: every phase energises exactly two coil inputs. */
    for (phase = 0; phase < STEPPER_PHASE_COUNT; phase++) {
        sum = 0;
        for (coil = 0; coil < 4; coil++) {
            uint8_t v = stepper_phase_coil(phase, coil);
            CHECK(v == 0 || v == 1);
            sum = (uint8_t)(sum + v);
        }
        CHECK(sum == 2);
    }

    /* Exact table (AIN1, AIN2, BIN1, BIN2). */
    CHECK(stepper_phase_coil(0, 0) == 1 && stepper_phase_coil(0, 1) == 0 &&
          stepper_phase_coil(0, 2) == 1 && stepper_phase_coil(0, 3) == 0);
    CHECK(stepper_phase_coil(1, 0) == 0 && stepper_phase_coil(1, 1) == 1 &&
          stepper_phase_coil(1, 2) == 1 && stepper_phase_coil(1, 3) == 0);
    CHECK(stepper_phase_coil(2, 0) == 0 && stepper_phase_coil(2, 1) == 1 &&
          stepper_phase_coil(2, 2) == 0 && stepper_phase_coil(2, 3) == 1);
    CHECK(stepper_phase_coil(3, 0) == 1 && stepper_phase_coil(3, 1) == 0 &&
          stepper_phase_coil(3, 2) == 0 && stepper_phase_coil(3, 3) == 1);

    /* Out-of-range phase wraps (mask), never reads out of bounds. */
    CHECK(stepper_phase_coil(4, 0) == stepper_phase_coil(0, 0));
}

static void test_phase_next(void)
{
    /* Forward: 0->1->2->3->0 */
    CHECK(stepper_phase_next(0, +1) == 1);
    CHECK(stepper_phase_next(1, +1) == 2);
    CHECK(stepper_phase_next(2, +1) == 3);
    CHECK(stepper_phase_next(3, +1) == 0);

    /* Reverse: 0->3->2->1->0 (mod 4) */
    CHECK(stepper_phase_next(0, -1) == 3);
    CHECK(stepper_phase_next(3, -1) == 2);
    CHECK(stepper_phase_next(2, -1) == 1);
    CHECK(stepper_phase_next(1, -1) == 0);

    /* Forward then reverse returns to the start. */
    CHECK(stepper_phase_next(stepper_phase_next(2, +1), -1) == 2);
}

static void test_dda_accumulation(void)
{
    int16_t acc;
    uint32_t total;
    uint32_t ticks;
    int i;

    /* 1000 steps/s -> exactly one step per 1 ms tick, no drift. */
    acc = 0;
    CHECK(stepper_dda_steps(&acc, 1000) == 1);
    CHECK(acc == 0);

    /* 500 steps/s -> a step every other tick (remainder carried). */
    acc = 0;
    CHECK(stepper_dda_steps(&acc, 500) == 0);
    CHECK(acc == 500);
    CHECK(stepper_dda_steps(&acc, 500) == 1);
    CHECK(acc == 0);

    /* 2000 / 2500 steps/s -> 2 steps, remainder preserved. */
    acc = 0;
    CHECK(stepper_dda_steps(&acc, 2000) == 2);
    CHECK(acc == 0);
    acc = 0;
    CHECK(stepper_dda_steps(&acc, 2500) == 2);
    CHECK(acc == 500);

    /* Maximum speed: 5 steps in one tick (the documented cap). */
    acc = 0;
    CHECK(stepper_dda_steps(&acc, MAX_STEP_SPEED) == 5);
    CHECK(acc == 0);
    acc = 999;
    CHECK(stepper_dda_steps(&acc, MAX_STEP_SPEED) == 5);
    CHECK(acc == 999); /* 999 + 5000 = 5999 -> 5 steps, remainder 999 */

    /* 1 step/s -> first step after 1000 ticks (999 empty ticks). */
    acc = 0;
    ticks = 0;
    while (stepper_dda_steps(&acc, 1) == 0)
        ticks++;
    CHECK(ticks == 999);

    /* Over a full second the produced steps equal the requested speed. */
    acc = 0;
    total = 0;
    for (i = 0; i < 1000; i++)
        total += stepper_dda_steps(&acc, 333);
    CHECK(total == 333);

    acc = 0;
    total = 0;
    for (i = 0; i < 1000; i++)
        total += stepper_dda_steps(&acc, 1);
    CHECK(total == 1);

    acc = 0;
    total = 0;
    for (i = 0; i < 1000; i++)
        total += stepper_dda_steps(&acc, MAX_STEP_SPEED);
    CHECK(total == MAX_STEP_SPEED);
}

static void test_speed_bounds(void)
{
    CHECK(stepper_clamp_speed(0, MAX_STEP_SPEED) == 1);        /* zero -> minimum */
    CHECK(stepper_clamp_speed(1, MAX_STEP_SPEED) == 1);
    CHECK(stepper_clamp_speed(MAX_STEP_SPEED, MAX_STEP_SPEED) == MAX_STEP_SPEED);
    CHECK(stepper_clamp_speed(MAX_STEP_SPEED + 1, MAX_STEP_SPEED) == MAX_STEP_SPEED);
    CHECK(stepper_clamp_speed(65535, MAX_STEP_SPEED) == MAX_STEP_SPEED);
}

static void test_move_target(void)
{
    int32_t t = 0;

    CHECK(stepper_move_target(0, 100, &t) == 0 && t == 100);
    CHECK(stepper_move_target(0, -100, &t) == 0 && t == -100);
    CHECK(stepper_move_target(50, 0, &t) == 0 && t == 50);

    /* int32 overflow must be rejected. */
    CHECK(stepper_move_target(2147483647, 1, &t) != 0);          /* INT32_MAX + 1 */
    CHECK(stepper_move_target(2147483647, -1, &t) == 0 && t == 2147483646);
    CHECK(stepper_move_target(-2147483647 - 1, -1, &t) != 0);    /* INT32_MIN - 1 */
    CHECK(stepper_move_target(-2147483647 - 1, 1, &t) == 0 && t == -2147483647);
    CHECK(stepper_move_target(2147483000, 1000, &t) != 0);
    CHECK(stepper_move_target(-2147483000, -1000, &t) != 0);
}

int main(void)
{
    test_phase_table();
    test_phase_next();
    test_dda_accumulation();
    test_speed_bounds();
    test_move_target();

    printf("%d checks, %d failed\n", tests_run, tests_failed);
    if (tests_failed == 0)
        printf("ALL TESTS PASSED\n");
    return tests_failed == 0 ? 0 : 1;
}