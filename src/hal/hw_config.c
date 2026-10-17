/**
  ******************************************************************************
  * @file    hw_config.c
  * @author  MCD Application Team
  * @version V2.3.0
  * @date    16-January-2014
  * @brief   Hardware configuration file
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; COPYRIGHT 2014 STMicroelectronics</center></h2>
  *
  * Licensed under MCD-ST Liberty SW License Agreement V2, (the "License");
  * You may not use this file except in compliance with the License.
  * You may obtain a copy of the License at:
  *
  *        http://www.st.com/software_license_agreement_liberty_v2
  *
  * Unless required by applicable law or agreed to in writing, software 
  * distributed under the License is distributed on an "AS IS" BASIS, 
  * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  * See the License for the specific language governing permissions and
  * limitations under the License.
  *
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "stm8s_conf.h"
#include "hw_config.h"
#include "main.h"

/* Private typedef ---------------------------------------------------------*/
/* Private define ----------------------------------------------------------*/
/* Private macro -------------------------------------------------------------*/
/* Private variables -----------------------------------------------------------*/

/* Private function prototypes -----------------------------------------------*/
/* Private functions -----------------------------------------------------------*/

/**
  * @brief  Initialize system clock
  * @param  None
  * @retval None
  */
void hw_clock_init(void)
{
    /* External resonator is NOT used. All clocking is from the internal HSI. */
    CLK_DeInit();
    CLK_HSICmd(ENABLE);
    while (CLK_GetFlagStatus(CLK_FLAG_HSIRDY) == RESET);
    CLK_ClockSwitchConfig(CLK_SWITCHMODE_AUTO, CLK_SOURCE_HSI, DISABLE, CLK_CURRENTCLOCKSTATE_DISABLE);

    /* Remove BOTH dividers: HSIDIV = /1 and CPUDIV = /1  ->  fMASTER = fHSI = 16 MHz.
       NOTE: CLK_SYSCLKConfig() only controls CPUDIV and leaves HSIDIV at its
       reset value (0b11 = HSI/8 = 2 MHz), which is far too slow. The spec
       requires writing CLK_CKDIVR = 0x00, so do exactly that. */
    CLK->CKDIVR = 0x00;
}

/**
  * @brief  Initialize GPIOs for the two DRV8833 H-bridge drivers.
  *
  * Each DRV8833 stepper channel needs four coil inputs (AIN1/AIN2 for coil A,
  * BIN1/BIN2 for coil B) plus nSLEEP (output) and nFAULT (input). There is no
  * STEP/DIR interface: the MCU sequences the coils itself (see stepper.c).
  *
  * TSSOP20 note: PB0-PB3 are NOT bonded out on the STM8S103F3P6 TSSOP20 (only
  * PB4/PB5 exist), so all coil/sleep/fault signals use port A/C/D pins.
  * @param  None
  * @retval None
  */
void hw_gpio_init(void)
{
    /* Configure all GPIOs */
    GPIO_DeInit(GPIOA);
    GPIO_DeInit(GPIOB);
    GPIO_DeInit(GPIOC);
    GPIO_DeInit(GPIOD);
    
    /* PB4/PB5 - I2C SCL/SDA (open-drain) */
    GPIO_Init(GPIOB, GPIO_PIN_4 | GPIO_PIN_5, GPIO_MODE_OUT_OD_HIZ_FAST);
    
    /* --- Focus DRV8833 ---
       PC3=AIN1, PC4=AIN2, PC5=BIN1, PC6=BIN2 (coil outputs, start coasting) */
    GPIO_Init(GPIOC, GPIO_PIN_3 | GPIO_PIN_4 | GPIO_PIN_5 | GPIO_PIN_6,
              GPIO_MODE_OUT_PP_LOW_FAST);
    /* PD3 - nSLEEP (LOW = driver in sleep) */
    GPIO_Init(GPIOD, GPIO_PIN_3, GPIO_MODE_OUT_PP_LOW_FAST);
    /* PD2 - nFAULT (open-drain, LOW on fault -> pull-up, read RESET means fault) */
    GPIO_Init(GPIOD, GPIO_PIN_2, GPIO_MODE_IN_PU_NO_IT);
    
    /* --- Zoom DRV8833 ---
       PD5=AIN1, PD6=AIN2, PA3=BIN1, PC7=BIN2 (coil outputs) */
    GPIO_Init(GPIOD, GPIO_PIN_5 | GPIO_PIN_6, GPIO_MODE_OUT_PP_LOW_FAST);
    GPIO_Init(GPIOA, GPIO_PIN_3, GPIO_MODE_OUT_PP_LOW_FAST);
    GPIO_Init(GPIOC, GPIO_PIN_7, GPIO_MODE_OUT_PP_LOW_FAST);
    /* PA1 - nSLEEP */
    GPIO_Init(GPIOA, GPIO_PIN_1, GPIO_MODE_OUT_PP_LOW_FAST);
    /* PA2 - nFAULT (pull-up) */
    GPIO_Init(GPIOA, GPIO_PIN_2, GPIO_MODE_IN_PU_NO_IT);
    
    /* PD4 - LED_STATUS (PP low) */
    GPIO_Init(GPIOD, GPIO_PIN_4, GPIO_MODE_OUT_PP_LOW_FAST);

    /* PD1 is intentionally left in its reset state (input floating): it is the
       SWIM data pin and must stay free for programming. */
}

/**
  * @brief  Initialize IWDG
  * @param  None
  * @retval None
  */
void hw_iwdg_init(void)
{
    IWDG_WriteAccessCmd(IWDG_WriteAccess_Enable);
    IWDG_SetPrescaler(IWDG_Prescaler_256);
    IWDG_SetReload(250); // ~500ms
    IWDG_ReloadCounter();
    IWDG_Enable();
}

/**
  * @brief  Initialize TIM4
  * @param  None
  * @retval None
  */
void hw_tim4_init(void)
{
    /* Enable TIM4 clock */
    CLK_PeripheralClockConfig(CLK_PERIPHERAL_TIMER4, ENABLE);
    
    /* Configure TIM4 time base for 1ms tick */
    TIM4_TimeBaseInit(TIM4_PRESCALER_128, 124);
    
    /* Enable update interrupt */
    TIM4_ITConfig(TIM4_IT_UPDATE, ENABLE);
    
    /* Enable TIM4 */
    TIM4_Cmd(ENABLE);
}

/**
  * @brief  Initialize I2C as slave
  * @param  None
  * @retval None
  */
void hw_i2c_init(void)
{
    /* Enable I2C clock */
    CLK_PeripheralClockConfig(CLK_PERIPHERAL_I2C, ENABLE);

    /* Single I2C configuration path: reuse the runtime re-init routine so the
       boot configuration and the error/timeout recovery can never diverge. */
    i2c_slave_reset();
}

/**
  * @brief  Reset system
  * @param  None
  * @retval None
  */
void hw_reset(void)
{
    hw_clock_init();
    hw_gpio_init();
    hw_iwdg_init();
    hw_tim4_init();
    hw_i2c_init();
}