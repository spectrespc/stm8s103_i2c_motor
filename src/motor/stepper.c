#include "main.h"
#include "stm8s_conf.h"
#include "stepper.h"
#include "stepper_logic.h"

/* Stepper motor array (focus = 0, zoom = 1) */
stepper_t steppers[2];

/* TIM4 ticks to wait after raising nSLEEP before emitting the first step, so
   the DRV8833 charge pump / outputs are valid (datasheet wake time tWAKE). */
#define STEPPER_WAKE_TICKS 1

/* Private helpers ----------------------------------------------------------*/

/**
  * @brief  Drive the four coil inputs of one motor to the coast state
  *         ({0,0} on both H-bridges: coils de-energised).
  */
static void stepper_coast(uint8_t motor)
{
    stepper_t *s = &steppers[motor];
    GPIO_WriteLow(s->ain1_port, s->ain1_pin);
    GPIO_WriteLow(s->ain2_port, s->ain2_pin);
    GPIO_WriteLow(s->bin1_port, s->bin1_pin);
    GPIO_WriteLow(s->bin2_port, s->bin2_pin);
}

/**
  * @brief  Apply the commutation pattern for the motor's current phase.
  */
static void stepper_apply_phase(uint8_t motor)
{
    stepper_t *s = &steppers[motor];
    uint8_t phase = s->phase;

    if (stepper_phase_coil(phase, 0)) GPIO_WriteHigh(s->ain1_port, s->ain1_pin);
    else                              GPIO_WriteLow (s->ain1_port, s->ain1_pin);
    if (stepper_phase_coil(phase, 1)) GPIO_WriteHigh(s->ain2_port, s->ain2_pin);
    else                              GPIO_WriteLow (s->ain2_port, s->ain2_pin);
    if (stepper_phase_coil(phase, 2)) GPIO_WriteHigh(s->bin1_port, s->bin1_pin);
    else                              GPIO_WriteLow (s->bin1_port, s->bin1_pin);
    if (stepper_phase_coil(phase, 3)) GPIO_WriteHigh(s->bin2_port, s->bin2_pin);
    else                              GPIO_WriteLow (s->bin2_port, s->bin2_pin);
}

/* Public functions ---------------------------------------------------------*/

void stepper_init(void)
{
    /* NOTE: pin numbers are TSSOP20 pins. PB0-PB3 are not bonded on this
       package, hence everything lives on ports A/C/D. */

    /* Focus DRV8833 */
    steppers[0].position = 0;
    steppers[0].target = 0;
    steppers[0].speed = 0;             /* no move until a speed is set */
    steppers[0].state = STATUS_IDLE;
    steppers[0].phase = 0;
    steppers[0].step_acc = 0;
    steppers[0].wake_ticks = 0;
    steppers[0].ain1_port = GPIOC; steppers[0].ain1_pin = GPIO_PIN_3;
    steppers[0].ain2_port = GPIOC; steppers[0].ain2_pin = GPIO_PIN_4;
    steppers[0].bin1_port = GPIOC; steppers[0].bin1_pin = GPIO_PIN_5;
    steppers[0].bin2_port = GPIOC; steppers[0].bin2_pin = GPIO_PIN_6;
    steppers[0].nsleep_port = GPIOD; steppers[0].nsleep_pin = GPIO_PIN_3;
    steppers[0].nfault_port = GPIOD; steppers[0].nfault_pin = GPIO_PIN_2;

    /* Zoom DRV8833 */
    steppers[1].position = 0;
    steppers[1].target = 0;
    steppers[1].speed = 0;
    steppers[1].state = STATUS_IDLE;
    steppers[1].phase = 0;
    steppers[1].step_acc = 0;
    steppers[1].wake_ticks = 0;
    steppers[1].ain1_port = GPIOD; steppers[1].ain1_pin = GPIO_PIN_5;
    steppers[1].ain2_port = GPIOD; steppers[1].ain2_pin = GPIO_PIN_6;
    steppers[1].bin1_port = GPIOA; steppers[1].bin1_pin = GPIO_PIN_3;
    steppers[1].bin2_port = GPIOC; steppers[1].bin2_pin = GPIO_PIN_7;
    steppers[1].nsleep_port = GPIOA; steppers[1].nsleep_pin = GPIO_PIN_1;
    steppers[1].nfault_port = GPIOA; steppers[1].nfault_pin = GPIO_PIN_2;

    /* Start with both drivers asleep and both motors coasting. */
    GPIO_WriteLow(steppers[0].nsleep_port, steppers[0].nsleep_pin);
    GPIO_WriteLow(steppers[1].nsleep_port, steppers[1].nsleep_pin);
    stepper_coast(0);
    stepper_coast(1);
}

void stepper_move(uint8_t motor, int16_t steps)
{
    stepper_t *s;
    int32_t new_target;

    if (motor >= 2) return;
    s = &steppers[motor];

    /* A move without a configured speed would report motion that never
       happens, so refuse it (the host sets the speed first). */
    if (s->speed == 0) return;
    if (steps == 0) return;

    /* Reject a move that would overflow int32_t (position persists across
       reboots, so overflow is reachable). */
    if (stepper_move_target(s->position, steps, &new_target) != 0)
        return;
    s->target = new_target;

    /* Wake the driver (nSLEEP high) and start stepping. The direction is
       derived from the sign of (target - position) inside stepper_tick().
       A move that is already RUNNING keeps stepping without a wake stall;
       a freshly woken driver waits STEPPER_WAKE_TICKS before the first step. */
    if (s->state != STATUS_RUNNING)
        s->wake_ticks = STEPPER_WAKE_TICKS;
    GPIO_WriteHigh(s->nsleep_port, s->nsleep_pin);
    s->state = STATUS_RUNNING;
}

void stepper_stop(uint8_t motor)
{
    stepper_t *s;

    if (motor >= 2) return;
    s = &steppers[motor];

    /* Coast (de-energise the coils) and put the driver back to sleep. */
    stepper_coast(motor);
    GPIO_WriteLow(s->nsleep_port, s->nsleep_pin);

    s->state = STATUS_IDLE;
    s->step_acc = 0;
    s->phase = 0;
    s->wake_ticks = 0;

    eeprom_dirty = 1; /* position changed -> persist it */
}

void stepper_set_speed(uint8_t motor, uint16_t speed)
{
    if (motor >= 2) return;
    steppers[motor].speed = stepper_clamp_speed(speed, MAX_STEP_SPEED);
}

/**
  * @brief  Advance both motors by the steps due this millisecond. Called from
  *         the TIM4 update ISR (1 ms). Also monitors nFAULT and latches an
  *         error (coast + sleep) if a driver reports a fault.
  */
void stepper_tick(void)
{
    uint8_t motor;
    uint8_t steps;

    for (motor = 0; motor < 2; motor++)
    {
        stepper_t *s = &steppers[motor];

        if (s->state != STATUS_RUNNING)
            continue;

        /* nFAULT is open-drain and LOW on fault (overcurrent/thermal/UVLO). */
        if (GPIO_ReadInputPin(s->nfault_port, s->nfault_pin) == RESET)
        {
            stepper_coast(motor);
            GPIO_WriteLow(s->nsleep_port, s->nsleep_pin);
            s->state = STATUS_ERROR;
            s->step_acc = 0;
            s->wake_ticks = 0;
            eeprom_dirty = 1;
            continue;
        }

        /* Let a freshly woken driver settle before the first step. */
        if (s->wake_ticks > 0)
        {
            s->wake_ticks--;
            continue;
        }

        /* DDA: emit the whole steps due at this motor's speed (<= 5 at max). */
        steps = stepper_dda_steps(&s->step_acc, s->speed);
        while (steps-- > 0)
        {
            int dir;

            if (s->position < s->target)
                dir = +1;
            else if (s->position > s->target)
                dir = -1;
            else
                break;

            s->phase = stepper_phase_next(s->phase, dir);
            s->position += dir;

            stepper_apply_phase(motor);

            if (s->position == s->target)
            {
                stepper_stop(motor);   /* sets IDLE, coasts, clears phase/acc */
                break;
            }
        }
    }
}