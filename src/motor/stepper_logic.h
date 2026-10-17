#ifndef STEPPER_LOGIC_H
#define STEPPER_LOGIC_H

#include <stdint.h>

/* Pure, host-testable stepper helpers. These contain NO STM8/SPL dependencies,
   so src/motor/stepper.c and the tests share exactly the same code and cannot
   drift apart. `static inline` keeps unused helpers from warning when a
   translation unit uses only some of them. */

#define STEPPER_PHASE_COUNT 4
#define STEPPER_DDA_BASE    1000  /* 1 ms tick -> base 1000 = steps/sec */

/* Clamp a requested speed to the 1..max_speed range. */
static inline uint16_t stepper_clamp_speed(uint16_t speed, uint16_t max_speed)
{
    if (speed == 0)
        return 1;
    if (speed > max_speed)
        return max_speed;
    return speed;
}

/* Compute the move target. Returns 0 on success, -1 if position + steps would
   overflow int32_t. Uses unsigned arithmetic so the two's-complement wrap is
   well defined (the firmware relies on the same wrap on the STM8). */
static inline int stepper_move_target(int32_t position, int16_t steps, int32_t *target)
{
    uint32_t wrapped = (uint32_t)position + (uint32_t)(int32_t)steps;
    int32_t  new_target = (int32_t)wrapped;

    if ((steps > 0 && new_target < position) ||
        (steps < 0 && new_target > position))
        return -1;

    *target = new_target;
    return 0;
}

/* Advance the DDA accumulator by `speed` (steps/sec) for one 1 ms tick.
   Returns the whole number of steps due this tick (0..speed/1000) and leaves
   the sub-step remainder in *acc. */
static inline uint8_t stepper_dda_steps(int16_t *acc, uint16_t speed)
{
    int16_t v = (int16_t)(*acc + (int16_t)speed);
    uint8_t steps = 0;

    while (v >= STEPPER_DDA_BASE) {
        v = (int16_t)(v - STEPPER_DDA_BASE);
        steps++;
    }
    *acc = v;
    return steps;
}

/* Next phase for a forward (dir > 0) or reverse (dir < 0) step. */
static inline uint8_t stepper_phase_next(uint8_t phase, int dir)
{
    return (uint8_t)((phase + ((dir > 0) ? 1 : (STEPPER_PHASE_COUNT - 1))) &
                     (STEPPER_PHASE_COUNT - 1));
}

/* Full-step (both coils energised) commutation table lookup.
   phase = 0..3, coil = 0..3 (AIN1, AIN2, BIN1, BIN2).
   DRV8833 per H-bridge: {1,0}=forward, {0,1}=reverse, {0,0}=coast, {1,1}=brake. */
static inline uint8_t stepper_phase_coil(uint8_t phase, uint8_t coil)
{
    static const uint8_t table[STEPPER_PHASE_COUNT][4] = {
        { 1, 0, 1, 0 },
        { 0, 1, 1, 0 },
        { 0, 1, 0, 1 },
        { 1, 0, 0, 1 },
    };
    return table[phase & (STEPPER_PHASE_COUNT - 1)][coil & 3];
}

#endif