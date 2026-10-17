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
  * @brief  Initialize GPIOs
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
    
    /* PC3 - FOCUS_STEP, PC6 - ZOOM_STEP (PP fast) */
    GPIO_Init(GPIOC, GPIO_PIN_3 | GPIO_PIN_6, GPIO_MODE_OUT_PP_LOW_FAST);
    
    /* PC4 - FOCUS_DIR, PC7 - ZOOM_DIR (PP high) */
    GPIO_Init(GPIOC, GPIO_PIN_4 | GPIO_PIN_7, GPIO_MODE_OUT_PP_HIGH_FAST);
    
    /* PC5 - FOCUS_EN, PD1 - ZOOM_EN (PP high) */
    GPIO_Init(GPIOC, GPIO_PIN_5, GPIO_MODE_OUT_PP_HIGH_FAST);
    GPIO_Init(GPIOD, GPIO_PIN_1, GPIO_MODE_OUT_PP_HIGH_FAST);
    
    /* PA1/PA2 - Limit switches (input with pull-up) */
    GPIO_Init(GPIOA, GPIO_PIN_1 | GPIO_PIN_2, GPIO_MODE_IN_PU_NO_IT);
    
    /* PD4 - LED_STATUS (PP low) */
    GPIO_Init(GPIOD, GPIO_PIN_4, GPIO_MODE_OUT_PP_LOW_FAST);
    
    /* Unused pins */
    GPIO_Init(GPIOB, GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3, GPIO_MODE_OUT_PP_LOW_SLOW);
    GPIO_Init(GPIOC, GPIO_PIN_1 | GPIO_PIN_2, GPIO_MODE_OUT_PP_LOW_SLOW);
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
  * @brief  Initialize TIM1
  * @param  None
  * @retval None
  */
void hw_tim1_init(void)
{
    /* Enable TIM1 clock */
    CLK_PeripheralClockConfig(CLK_PERIPHERAL_TIMER1, ENABLE);
    
    /* Configure TIM1 time base */
    TIM1_TimeBaseInit(0, TIM1_COUNTERMODE_UP, 3199, 0); // Prescaler=0, Period=3199, Repetition=0
    
    /* Configure TIM1 channels for PWM */
    TIM1_OC1Init(TIM1_OCMODE_PWM1, TIM1_OUTPUTSTATE_ENABLE, TIM1_OUTPUTNSTATE_DISABLE, 0, TIM1_OCPOLARITY_HIGH, TIM1_OCNPOLARITY_HIGH, TIM1_OCIDLESTATE_SET, TIM1_OCNIDLESTATE_SET);
    TIM1_OC3Init(TIM1_OCMODE_PWM1, TIM1_OUTPUTSTATE_ENABLE, TIM1_OUTPUTNSTATE_DISABLE, 0, TIM1_OCPOLARITY_HIGH, TIM1_OCNPOLARITY_HIGH, TIM1_OCIDLESTATE_SET, TIM1_OCNIDLESTATE_SET);
    
    /* Enable PWM outputs */
    TIM1_CtrlPWMOutputs(ENABLE);
    
    /* Enable update interrupt */
    TIM1_ITConfig(TIM1_IT_UPDATE, ENABLE);
    
    /* Enable TIM1 */
    TIM1_Cmd(ENABLE);
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
    hw_tim1_init();
    hw_tim4_init();
    hw_i2c_init();
}