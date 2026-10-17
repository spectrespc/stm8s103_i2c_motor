#include "main.h"
#include "stm8s_conf.h"
#include "stepper.h"

/* Stepper motor array */
stepper_t steppers[2];

/* Stepper motor control functions */
void stepper_init(void)
{
    /* Initialize stepper motors (TIM1 already configured by hw_tim1_init) */
    steppers[0].position = 0;
    steppers[0].target = 0;
    steppers[0].speed = 0;
    steppers[0].state = 0; /* IDLE */
    steppers[0].dir_pin = GPIO_PIN_4; /* FOCUS_DIR */
    steppers[0].en_pin = GPIO_PIN_5; /* FOCUS_EN */
    steppers[0].limit_pin = GPIO_PIN_1; /* FOCUS_LIMIT */
    steppers[0].limit_debounce = 0;
    steppers[0].limit_stable = 0;
    steppers[0].tim1_channel = 3; /* FOCUS_STEP on PC3 */
    steppers[0].calibrating = 0;
    
    steppers[1].position = 0;
    steppers[1].target = 0;
    steppers[1].speed = 0;
    steppers[1].state = 0; /* IDLE */
    steppers[1].dir_pin = GPIO_PIN_7; /* ZOOM_DIR */
    steppers[1].en_pin = GPIO_PIN_1; /* ZOOM_EN */
    steppers[1].limit_pin = GPIO_PIN_2; /* ZOOM_LIMIT */
    steppers[1].limit_debounce = 0;
    steppers[1].limit_stable = 0;
    steppers[1].tim1_channel = 1; /* ZOOM_STEP on PC6 */
    steppers[1].calibrating = 0;
}

void stepper_move(uint8_t motor, int16_t steps)
{
    int32_t new_target;

    if (motor >= 2) return;

    /* Refuse to move when no speed has been configured. With speed == 0 the
       PWM (CCR) stays 0 so no STEP pulses are emitted, yet stepper_tick()
       would still count position - the reported position would diverge from
       the physical motor. Requiring a speed keeps them consistent. */
    if (steppers[motor].speed == 0)
        return;

    /* Compute the target and reject a move that would overflow int32_t
       (position persists across reboots, so overflow is reachable). */
    new_target = steppers[motor].position + (int32_t)steps;
    if ((steps > 0 && new_target < steppers[motor].position) ||
        (steps < 0 && new_target > steppers[motor].position))
        return;
    steppers[motor].target = new_target;

    /* Set direction for target motor only */
    if (steps > 0)
    {
        if (motor == 0)
        {
            GPIO_WriteHigh(GPIOC, GPIO_PIN_4); /* FOCUS_DIR */
        }
        else
        {
            GPIO_WriteHigh(GPIOC, GPIO_PIN_7); /* ZOOM_DIR */
        }
    }
    else if (steps < 0)
    {
        if (motor == 0)
        {
            GPIO_WriteLow(GPIOC, GPIO_PIN_4); /* FOCUS_DIR */
        }
        else
        {
            GPIO_WriteLow(GPIOC, GPIO_PIN_7); /* ZOOM_DIR */
        }
    }
    
    /* Set motor enable (active low) */
    if (motor == 0)  GPIO_WriteLow(GPIOC, GPIO_PIN_5);  /* FOCUS_EN */
    else             GPIO_WriteLow(GPIOD, GPIO_PIN_1);   /* ZOOM_EN */
    
    /* Set speed */
    if (steppers[motor].speed > 0)
    {
        uint16_t arr = 16000000 / steppers[motor].speed - 1;
        if (arr < 1000) arr = 1000;
        uint16_t ccr = arr / 2;
        
        if (motor == 0)
        {
            /* FOCUS motor on TIM1_CH3 (PC3) */
            TIM1->ARRH = (arr >> 8) & 0xFF;
            TIM1->ARRL = arr & 0xFF;
            TIM1->CCR3H = (ccr >> 8) & 0xFF;
            TIM1->CCR3L = ccr & 0xFF;
        }
        else
        {
            /* ZOOM motor on TIM1_CH1 (PC6) */
            TIM1->ARRH = (arr >> 8) & 0xFF;
            TIM1->ARRL = arr & 0xFF;
            TIM1->CCR1H = (ccr >> 8) & 0xFF;
            TIM1->CCR1L = ccr & 0xFF;
        }
    }
    
    steppers[motor].state = STATUS_RUNNING;
}

void stepper_stop(uint8_t motor)
{
    if (motor >= 2) return;
    
    /* Disable motor (active low) */
    if (motor == 0)
    {
        GPIO_WriteHigh(GPIOC, GPIO_PIN_5); /* FOCUS_EN */
    }
    else
    {
        GPIO_WriteHigh(GPIOD, GPIO_PIN_1); /* ZOOM_EN */
    }
    
    /* Set state to IDLE */
    steppers[motor].state = 0; /* IDLE */
    steppers[motor].calibrating = 0;    /* a stopped motor is no longer calibrating */
    steppers[motor].limit_debounce = 0;
    eeprom_dirty = 1; /* Mark EEPROM as dirty for saving */
}

void stepper_set_speed(uint8_t motor, uint16_t speed)
{
    if (motor >= 2) return;
    
    /* Clamp speed */
    if (speed == 0)
    {
        speed = 1; /* Minimum speed */
    }
    if (speed > MAX_STEP_SPEED)
    {
        speed = MAX_STEP_SPEED; /* Maximum speed */
    }
    
    steppers[motor].speed = speed;
}

void stepper_calibrate(uint8_t motor, uint16_t speed)
{
    if (motor >= 2) return;

    /* Validate the speed exactly like stepper_set_speed(): a zero speed would
       cause a division by zero below, and the max prevents runaway motion. */
    if (speed == 0)
        speed = 1;
    if (speed > MAX_STEP_SPEED)
        speed = MAX_STEP_SPEED;

    /* Set direction to move towards limit switch (active low) */
    if (motor == 0)
        GPIO_WriteLow(GPIOC, GPIO_PIN_4); /* FOCUS_DIR towards limit */
    else
        GPIO_WriteLow(GPIOC, GPIO_PIN_7); /* ZOOM_DIR towards limit */
    
    /* Set enable */
    if (motor == 0)  GPIO_WriteLow(GPIOC, GPIO_PIN_5); /* FOCUS_EN */
    else             GPIO_WriteLow(GPIOD, GPIO_PIN_1); /* ZOOM_EN */
    
    steppers[motor].speed = speed;
    
    /* Mark as calibrating so tick() sets position=0 on limit hit */
    steppers[motor].calibrating = 1;
    steppers[motor].state = STATUS_RUNNING;
    
    /* Start PWM for the motor */
    uint16_t arr = 16000000 / speed - 1;
    if (arr < 1000) arr = 1000;
    uint16_t ccr = arr / 2;
    
    if (motor == 0) {
        TIM1->ARRH = (arr >> 8) & 0xFF;
        TIM1->ARRL = arr & 0xFF;
        TIM1->CCR3H = (ccr >> 8) & 0xFF;
        TIM1->CCR3L = ccr & 0xFF;
    } else {
        TIM1->ARRH = (arr >> 8) & 0xFF;
        TIM1->ARRL = arr & 0xFF;
        TIM1->CCR1H = (ccr >> 8) & 0xFF;
        TIM1->CCR1L = ccr & 0xFF;
    }
}

void stepper_tick(void)
{
    uint8_t motor;

    for (motor = 0; motor < 2; motor++)
    {
        uint8_t limit_hit;

        if (steppers[motor].state != STATUS_RUNNING)
            continue;

        limit_hit = (GPIO_ReadInputPin(GPIOA, steppers[motor].limit_pin) == RESET);

        if (steppers[motor].calibrating)
        {
            /* Calibration: move continuously toward the limit switch (DIR was
               set by stepper_calibrate()). There is no target position - the
               limit switch ends the motion. */
            if (steppers[motor].position > CALIB_MIN_POS)
                steppers[motor].position--;
            else
            {
                /* Travel guard: no limit switch reached within the allowed
                   range - stop as if the limit was found. */
                steppers[motor].position = 0;
                steppers[motor].target = 0;
                steppers[motor].calibrating = 0;
                stepper_stop(motor);
                continue;
            }

            if (limit_hit)
            {
                if (steppers[motor].limit_debounce < DEBOUNCE_MS)
                    steppers[motor].limit_debounce++;
                else
                {
                    steppers[motor].position = 0;
                    steppers[motor].target = 0;
                    steppers[motor].calibrating = 0;
                    steppers[motor].limit_debounce = 0;
                    stepper_stop(motor);          /* sets state = IDLE */
                }
            }
            else
            {
                steppers[motor].limit_debounce = 0;
            }
            continue;
        }

        /* Normal positioned move: one step per PWM pulse toward the target. */
        if (steppers[motor].position < steppers[motor].target)
            steppers[motor].position++;
        else if (steppers[motor].position > steppers[motor].target)
            steppers[motor].position--;

        if (limit_hit)
        {
            if (steppers[motor].limit_debounce < DEBOUNCE_MS)
                steppers[motor].limit_debounce++;
            else
            {
                /* Hard limit reached during a normal move: stop immediately. */
                stepper_stop(motor);
                steppers[motor].state = STATUS_LIMIT;
                continue;
            }
        }
        else
        {
            steppers[motor].limit_debounce = 0;
        }

        /* Target reached */
        if (steppers[motor].position == steppers[motor].target)
            stepper_stop(motor);
    }
}