/*
 * boot_hw.c - minimal register-level hardware setup for the bootloader.
 *
 * No SPL library object is linked into the boot image: the same headers are
 * used, but every peripheral is configured through its registers.  This keeps
 * the bootloader inside the 2 KiB slot at 0x8000.
 */
#include "boot.h"

/* Motor driver safety: nSLEEP low, all coil inputs low.  The bootloader never
 * moves a motor, but it must not leave a driver awake after a reset. */
static void boot_gpio_init(void)
{
#if defined(BOARD_TEST)
    /* Test bench wiring: PA1 = focus nSLEEP, PA3 = zoom nSLEEP.
     * DDR=1, CR1=0 -> push-pull output, ODR=0 -> LOW (driver asleep).
     * Do NOT copy the historical PD3 sequence below: it clears DDR and only
     * touches CR1, which leaves the pin a floating input. */
    GPIOA->DDR |= (uint8_t)((1u << 1) | (1u << 3));
    GPIOA->CR1 &= (uint8_t)~((1u << 1) | (1u << 3));
    GPIOA->ODR &= (uint8_t)~((1u << 1) | (1u << 3));

    /* PA2 = focus nFAULT, PD6 = zoom nFAULT: input + pull-up (DDR=0, CR1=1,
     * CR2=0).  Coil lines stay in their reset floating-input state: the
     * DRV8833 coil inputs have 150k pulldowns, so the outputs coast. */
    GPIOA->DDR &= (uint8_t)~(1u << 2);
    GPIOA->CR1 |= (uint8_t)(1u << 2);
    GPIOD->DDR &= (uint8_t)~(1u << 6);
    GPIOD->CR1 |= (uint8_t)(1u << 6);
#else
    /* PD2 = focus nFAULT (pull-up input), PD3 = focus nSLEEP (low output) */
    GPIOD->DDR &= (uint8_t)~((1u << 2) | (1u << 3));
    GPIOD->CR1 |= (uint8_t)((1u << 2) | (1u << 3));
    GPIOD->CR1 &= (uint8_t)~(1u << 3);   /* PD3 push-pull */
    GPIOD->CR2 |= (uint8_t)((1u << 2) | (1u << 3));
    GPIOD->ODR &= (uint8_t)~(1u << 3);   /* nSLEEP = 0 */

    /* PA1 = zoom nSLEEP (low output).  nFAULT is left as a reset input:
     * with both nSLEEP pins low the DRV8833 outputs are disabled. */
    GPIOA->DDR |= (uint8_t)(1u << 1);
    GPIOA->CR1 |= (uint8_t)(1u << 1);
    GPIOA->ODR &= (uint8_t)~(1u << 1);
#endif

    /* PB4/PB5: open-drain outputs for I2C (external 4.7k pull-ups). */
    GPIOB->DDR |= (uint8_t)((1u << 4) | (1u << 5));
    GPIOB->CR1 &= (uint8_t)~((1u << 4) | (1u << 5));
    GPIOB->CR2 |= (uint8_t)((1u << 4) | (1u << 5));
    GPIOB->ODR |= (uint8_t)((1u << 4) | (1u << 5));
}

static void boot_i2c_init(void)
{
    /* fMASTER = 16 MHz, 100 kHz bus.  FREQR = 16 is the SPL value used by the
     * application too; the slave does not divide the bus itself. */
    CLK->PCKENR1 |= (uint8_t)CLK_PCKENR1_I2C;

    I2C->CR1 = 0x00;                 /* disable while reconfiguring */
    I2C->FREQR = 16;
    I2C->OARL = (uint8_t)((0x60u << 1) | 0x01u); /* 7-bit address, ADDCONF */
    I2C->OARH = 0x00;
    I2C->ITR = 0x00;                 /* polling: no interrupt requests */
    I2C->CCRH = 0x00;
    I2C->CCRL = 0x50;                /* 100 kHz @16 MHz (dummy for slave) */
    I2C->TRISER = 0x00;
    /* PE first, then ACK: the peripheral only latches ACK while enabled.
     * This order matches the application's i2c_slave_reset(). */
    I2C->CR1 = 0x01;                 /* PE = 1 */
    I2C->CR2 = 0x04;                 /* ACK enable */
}

void boot_hw_init(void)
{
    /* HSI / 1 and CPU / 1: 16 MHz.  Writing CKDIVR directly is required:
     * CLK_SYSCLKConfig() leaves HSIDIV at its reset value (HSI/8). */
    CLK->CKDIVR = 0x00;
    CLK->ICKR |= 0x01;               /* HSION */

    boot_gpio_init();

    /* Independent watchdog, ~500 ms. */
    IWDG->KR = 0xCC;                 /* enable write access */
    IWDG->KR = 0x55;                 /* unlock PR/RLR */
    IWDG->PR = 0x06;                 /* /256, ~500 Hz */
    IWDG->RLR = 250;                 /* ~500 ms; long loops feed it */
    IWDG->KR = 0xAA;                 /* reload */

    boot_i2c_init();
}