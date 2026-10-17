/**
  ******************************************************************************
  * @file    main.c
  * @author  MCD Application Team
  * @version V2.3.0
  * @date    16-January-2014
  * @brief   Main program body
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
#include "stm8s_it.h"    /* SDCC patch: REQUIRED so SDCC emits the interrupt
                            vector table in this module (the one with main()).
                            Without it only the reset vector is generated and
                            no ISR (I2C/TIM1/TIM4) ever fires. */
#include "main.h"

/* Private variables ---------------------------------------------------------*/
/* Volatile: written from ISRs (TIM4/I2C/TIM1) and read in the main loop. */
volatile uint32_t system_ticks = 0;
volatile uint32_t i2c_last_activity = 0;
volatile uint8_t i2c_need_reset = 0;
volatile uint8_t i2c_cmd_ready = 0;
uint32_t eeprom_last_save_time = 0;
volatile uint8_t eeprom_dirty = 0;

/* Private function prototypes ---------------------------------------*/
/* Private functions ---------------------------------------------------------*/

/**
  * @brief  Main program.
  * @param  None
  * @retval None
  */
void main(void)
{
    /* Initialize hardware */
    hw_reset();

    /* Initialise the application modules. These MUST be called: stepper_init()
       fills in the per-motor pin/channel fields (a zeroed limit_pin would make
       the limit check read the wrong pin and stop every move), and
       eeprom_init() restores the saved positions. */
    stepper_init();
    eeprom_init();
    
    /* Enable interrupts */
    __asm__("rim");
    
    uint8_t led_state = 0;
    uint32_t last_led_tick = 0;
    
    /* Infinite loop */
    while (1)
    {
        IWDG_ReloadCounter();
        
        if (i2c_need_reset)
        {
            i2c_slave_reset();
            i2c_need_reset = 0;
        }
        
        if (i2c_check_timeout())
            i2c_slave_reset();
        
        if (i2c_cmd_ready)
        {
            __asm__("sim");
            i2c_process_command();
            i2c_cmd_ready = 0;
            __asm__("rim");
        }
        
        eeprom_periodic_save();
        
        /* LED blink every 500ms */
        if (system_ticks - last_led_tick >= 500)
        {
            last_led_tick = system_ticks;
            led_state = !led_state;
            if (led_state)
                GPIO_WriteHigh(GPIOD, GPIO_PIN_4);
            else
                GPIO_WriteLow(GPIOD, GPIO_PIN_4);
        }
    }
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t* file, uint32_t line)
{
    /* User can add his own implementation to report the file name and line number */
    while (1)
    {
    }
}
#endif